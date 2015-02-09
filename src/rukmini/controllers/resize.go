package controllers

import (
	"bytes"
	"fmt"
	"github.com/astaxie/beego"
	"github.com/chai2010/webp"
	"github.com/gographics/imagick/imagick"
	"github.com/nfnt/resize"
	"github.com/satori/uuid"
	"image"
	"image/gif"
	"image/jpeg"
	"image/png"
	"io/ioutil"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"
)

type ResizeController struct {
	beego.Controller
}

type ResizeParameters struct {
	what        string
	width       float64
	height      float64
	quality     int
	uri         string
	render_webp bool
}

var imageMagickConcurrencyLock sync.RWMutex

var timeout = time.Duration(3 * time.Second)

func dialTimeout(network, addr string) (net.Conn, error) {
	return net.DialTimeout(network, addr, timeout)
}

var transport = &http.Transport{MaxIdleConnsPerHost: 64, DisableKeepAlives: true, Dial: dialTimeout}

var client = &http.Client{Transport: transport, Timeout: timeout}

func (this *ResizeController) ExtractParameters() (*ResizeParameters, error) {
	what := this.Ctx.Input.Param(":what")
	width, err := strconv.ParseFloat(this.Ctx.Input.Param(":width"), 64)
	var convertedWidth = width
	if err != nil {
		return &ResizeParameters{}, err
	}
	height, err := strconv.ParseFloat(this.Ctx.Input.Param(":height"), 64)
	var convertedHeight = height
	if err != nil {
		return &ResizeParameters{}, err
	}
	imageUri := this.Ctx.Input.Param(":path")
	var quality int = 90
	if len(this.Input().Get("q")) != 0 {
		qt, e := strconv.Atoi(this.Input().Get("q"))
		if e != nil {
			quality = 90
		} else {
			quality = qt
		}
	}
	var render_webp bool = false
	if len(this.Input().Get("webp")) != 0 {
		wp, e := strconv.ParseBool(this.Input().Get("webp"))
		if e == nil {
			render_webp = wp
		}
	}
	return &ResizeParameters{what: what, width: convertedWidth, height: convertedHeight, quality: quality, uri: imageUri, render_webp: render_webp}, nil
}

//Add all the caching headers here
func AddCacheHeaders(this *ResizeController) {
	maxAge, err := beego.AppConfig.Int64("resource.maxage")
	if err != nil {
		this.Ctx.ResponseWriter.Header().Add("Cache-Control", "max-age="+strconv.FormatInt(maxAge, 10))
	} else {
		this.Ctx.ResponseWriter.Header().Add("Cache-Control", "max-age=63072000")
	}
}

/**
 * Resize the image and maintain aspect ratio
 */
func (this *ResizeController) Get() {
	resizeParameters, err := this.ExtractParameters()
	if err != nil {
		logAccess(this, 400, 0)
		this.Abort("400")
		return
	}
	u4 := uuid.NewV4()
	fileExt := filepath.Ext(resizeParameters.uri)
	fileName := fmt.Sprintf("/tmp/%s%s", u4, fileExt)
	downloadUrl := fmt.Sprintf("%s%s", beego.AppConfig.String(resizeParameters.what+".source"), resizeParameters.uri)
	req, _ := http.NewRequest("GET", downloadUrl, nil)
	imageDownloadResponse, err := client.Do(req)
	if err != nil {
		logAccess(this, 500, 0)
		this.Abort("500")
		return
	}
	imageData, err := ioutil.ReadAll(imageDownloadResponse.Body)
	responseStatusCode := imageDownloadResponse.StatusCode
	imageDownloadResponse.Body.Close()
	imageDownloadResponse = nil
	if responseStatusCode == 200 || responseStatusCode == 302 || responseStatusCode == 304 {
		if err = ioutil.WriteFile(fileName, imageData, os.ModePerm); err != nil {
			errMessage := fmt.Sprintf("Image Download Error: %s", err)
			beego.Warn(errMessage)
			logAccess(this, 500, 0)
			this.Abort("500")
			return
		}
		// open "test.jpg"
		originalImageFile, err := os.Open(fileName)
		if err != nil {
			errMessage := fmt.Sprintf("Image Download Error: %s", err)
			beego.Warn(errMessage)
			logAccess(this, 500, 0)
			this.Abort("500")
			return
		}
		// try to decode the image
		originalImg, _, err := image.Decode(originalImageFile)
		if err != nil {
			errMessage := fmt.Sprintf("Image Decode Error. Fallback to ImageMagick: %s", err)
			beego.Warn(errMessage)
			originalImageFile.Close()
			resizeUsingImageMagick(this, fileName, resizeParameters.width, resizeParameters.height, resizeParameters.quality, downloadUrl)
		} else {
			originalImageFile.Close()
			originalImageFile1, err := os.Open(fileName)
			imgc, _, err := image.DecodeConfig(originalImageFile1)
			if err != nil {
				errMessage := fmt.Sprintf("Image Get Decode Config Error. Fallback to ImageMagick: %s", err)
				beego.Warn(errMessage)
				//			this.Ctx.Abort(500, errMessage)
				originalImageFile1.Close()
				//			os.Remove(fileName)
				resizeUsingImageMagick(this, fileName, resizeParameters.width, resizeParameters.height, resizeParameters.quality, downloadUrl)
			} else {
				originalImageFile1.Close()
				var original_width = imgc.Width
				var original_height = imgc.Height
				if float64(original_height) <= resizeParameters.height && float64(original_width) <= resizeParameters.width {
					logAccess(this, 200, 0)
					AddCacheHeaders(this)
					http.ServeFile(this.Ctx.ResponseWriter, this.Ctx.Request, fileName)
					os.Remove(fileName)
					return
				}
				os.Remove(fileName)
				//Preserve aspect ratio
				width_ratio := resizeParameters.width / float64(original_width)
				height_ratio := resizeParameters.height / float64(original_height)
				var width = float64(-1)
				var height = float64(-1)
				if width_ratio < height_ratio {
					width = float64(original_width) * width_ratio
					height = float64(original_height) * width_ratio
				} else {
					width = float64(original_width) * height_ratio
					height = float64(original_height) * height_ratio
				}
				if width < 1 {
					width = 1
				}
				if height < 1 {
					height = 1
				}
				resizedImage := resize.Resize(uint(width), uint(height), originalImg, resize.Lanczos3)
				resizeImageFile, err := os.Create(fileName)
				if err != nil {
					errMessage := fmt.Sprintf("Image Resize Error: %s", err)
					beego.Warn(errMessage)
					logAccess(this, 500, 0)
					this.Abort("500")
					os.Remove(fileName)
					return
				}
				if resizeParameters.render_webp == false {
					if fileExt == ".jpeg" || fileExt == ".jpg" {
						jpeg.Encode(resizeImageFile, resizedImage, &jpeg.Options{Quality: resizeParameters.quality})
					}
					if fileExt == ".png" {
						png.Encode(resizeImageFile, resizedImage)
					}
					if fileExt == ".gif" {
						gif.Encode(resizeImageFile, resizedImage, &gif.Options{NumColors: 256})
					}
				} else {
					var buf bytes.Buffer
					if err = webp.Encode(&buf, resizedImage, &webp.Options{Lossless: false, Quality: float32(resizeParameters.quality)}); err != nil {
						logAccess(this, 500, 0)
						this.Abort("500")
						os.Remove(fileName)
						return
					}
					if err = ioutil.WriteFile(fileName+".webp", buf.Bytes(), 0666); err != nil {
						logAccess(this, 500, 0)
						this.Abort("500")
						os.Remove(fileName)
						return
					}
				}
				if resizeParameters.render_webp == true {
					AddCacheHeaders(this)
					http.ServeFile(this.Ctx.ResponseWriter, this.Ctx.Request, fileName+".webp")
					os.Remove(fileName)
					return
				} else {
					stat, err := resizeImageFile.Stat()
					if err != nil {
						errMessage := fmt.Sprintf("Image Resize/Stat Error: %s", err)
						beego.Warn(errMessage)
						logAccess(this, 500, 0)
						this.Abort("500")
						resizeImageFile.Close()
						os.Remove(fileName)
						return
					}
					resizeImageFile.Close()
					fSize := stat.Size()
					if fSize < 100 {
						errMessage := fmt.Sprintf("Image Size Error: %s", err)
						beego.Warn(errMessage)
						logAccess(this, 500, 0)
						this.Abort("500")
						os.Remove(fileName)
						return
					}
					logAccess(this, 200, fSize)
					AddCacheHeaders(this)
					http.ServeFile(this.Ctx.ResponseWriter, this.Ctx.Request, fileName)
					os.Remove(fileName)
					return
				}
			}
		}
	} else {
		//		beego.Error("Error downloading file: " +downloadUrl +" Status: " +strconv.Itoa(response.StatusCode) +"[" +response.Status +"]")
		logAccess(this, 500, 0)
		this.Abort("500")
		return
	}
}

func resizeUsingImageMagick(this *ResizeController, fileName string, width float64, height float64, quality int, downloadUrl string) {
	//Serialize imagemagick calls
	imageMagickConcurrencyLock.Lock()
	defer imageMagickConcurrencyLock.Unlock()

	imagick.Initialize()
	defer imagick.Terminate()
	mw := imagick.NewMagickWand()
	defer mw.Destroy()
	err := mw.ReadImage(fileName)
	if err != nil {
		logAccess(this, 500, 0)
		this.Abort("500")
		os.Remove(fileName)
		return
	}
	// Get original logo size
	original_width := mw.GetImageWidth()
	original_height := mw.GetImageHeight()
	width_ratio := width / float64(original_width)
	height_ratio := height / float64(original_height)
	if width_ratio < height_ratio {
		width = float64(original_width) * width_ratio
		height = float64(original_height) * width_ratio
	} else {
		width = float64(original_width) * height_ratio
		height = float64(original_height) * height_ratio
	}
	if width < 1 {
		width = 1
	}
	if height < 1 {
		height = 1
	}
	if quality < 1 {
		quality = 90
	}
	//	beego.Info(fmt.Sprintf("Image: %s | Size: %d X %d -> %4.f X %4.f | Width Ration: %4.4f | Height Ratio: %4.4f | Quality: %d", downloadUrl, original_width,original_height, width, height, width_ratio, height_ratio, quality))
	// Resize the image using the Lanczos filter
	// The blur factor is a float, where > 1 is blurry, < 1 is sharp
	err = mw.ResizeImage(uint(width), uint(height), imagick.FILTER_LANCZOS, 1)
	if err != nil {
		logAccess(this, 500, 0)
		this.Abort("500")
		os.Remove(fileName)
		return
	}
	err = mw.SetImageCompressionQuality(uint(quality))
	if err != nil {
		logAccess(this, 500, 0)
		this.Abort("500")
		os.Remove(fileName)
		return
	}
	mw.SetImageInterlaceScheme(imagick.INTERLACE_PLANE)
	mw.SetImageFormat(strings.Replace(filepath.Ext(fileName), ".", "", -1))
	mw.WriteImage(fileName)
	resizeImageFile, err := os.Open(fileName)
	if err != nil {
		logAccess(this, 500, 0)
		this.Abort("500")
		os.Remove(fileName)
		return
	}
	stat, err := resizeImageFile.Stat()
	if err != nil {
		logAccess(this, 500, 0)
		this.Abort("500")
		resizeImageFile.Close()
		os.Remove(fileName)
		return
	}
	resizeImageFile.Close()
	fSize := stat.Size()
	if fSize < 100 {
		logAccess(this, 500, fSize)
		this.Abort("500")
		os.Remove(fileName)
		return
	}
	AddCacheHeaders(this)
	http.ServeFile(this.Ctx.ResponseWriter, this.Ctx.Request, fileName)
	logAccess(this, 200, fSize)
	os.Remove(fileName)
	return
}

func logAccess(this *ResizeController, status int, size int64) {
	clientIp := this.Ctx.Request.Header.Get("FK-Client-IP")
	beego.Info(clientIp, this.Ctx.Request.UserAgent(), this.Ctx.Request.Method, this.Ctx.Request.RequestURI, this.Ctx.Request.Proto, status, size)
}
