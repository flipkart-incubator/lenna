package controllers

import (
	"fmt"
	"io/ioutil"
	"os"
	"net/http"
	"strconv"
	"github.com/astaxie/beego"
	"github.com/satori/uuid"
	"github.com/nfnt/resize"
	"image"
	"image/jpeg"
	"image/png"
	"image/gif"
	_ "image/png"
	_ "image/jpeg"
	_ "image/gif"
	"path/filepath"
	"sync"
	"github.com/gographics/imagick/imagick"
	"strings"
)

type ResizeController struct {
	beego.Controller
}

var imageMagickConcurrencyLock sync.RWMutex

var transport = &http.Transport{MaxIdleConnsPerHost: 64}

var client = &http.Client{Transport: transport}

/**
 * Resize the image and maintain aspect ratio
 */
func (this *ResizeController) Get() {
	//Source key that is in config which points to a source host
	what := this.Ctx.Input.Param(":what")
	width, err := strconv.ParseFloat(this.Ctx.Input.Param(":width"), 64)
	if width < 0 || err != nil {
		this.Ctx.Abort(400, "Invalid width specified")
		return
	}
	height, err := strconv.ParseFloat(this.Ctx.Input.Param(":height"), 64)
	if height < 0 || err != nil {
		this.Ctx.Abort(400, "Invalid height specified")
		return
	}
	imageUri := this.Ctx.Input.Param(":splat")
	var quality int = 90
	if len(this.Input().Get("q")) != 0 {
		qt, e := strconv.Atoi(this.Input().Get("q"))
		if e != nil {
			quality = 90
		} else {
			quality = qt
		}
	}
	u4 := uuid.NewV4()
	fileExt := filepath.Ext(imageUri)
	fileName := fmt.Sprintf("/tmp/%s%s", u4, fileExt)
	downloadUrl := fmt.Sprintf("%s%s", beego.AppConfig.String(what +".source"), imageUri)
	response, err := client.Get(downloadUrl)
	defer response.Body.Close()
	if err != nil {
		errMessage := fmt.Sprintf("%s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		return
	}
	if response.StatusCode == 200 || response.StatusCode == 302 || response.StatusCode == 304 {
		imageData, err := ioutil.ReadAll(response.Body)
		if err = ioutil.WriteFile(fileName, imageData, os.ModePerm); err != nil {
			errMessage := fmt.Sprintf("%s", err)
			beego.Error(errMessage)
			this.Ctx.Abort(500, errMessage)
			return
		}
		// open "test.jpg"
		originalImageFile, err := os.Open(fileName)
		if err != nil {
			errMessage := fmt.Sprintf("Image Open Error: %s", err)
			beego.Error(errMessage)
			this.Ctx.Abort(500, errMessage)
			return
		}
		// try to decode the image
		originalImg, _, err := image.Decode(originalImageFile)
		if err != nil {
			errMessage := fmt.Sprintf("Image Decode Error. Fallback to ImageMagick: %s", err)
			beego.Warn(errMessage)
			originalImageFile.Close()
			resizeUsingImageMagick(this, fileName, width, height, quality, downloadUrl)
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
				resizeUsingImageMagick(this, fileName, width, height, quality, downloadUrl)
			} else {
				originalImageFile1.Close()
				var original_width = imgc.Width
				var original_height = imgc.Height
				if float64(original_height) <= height && float64(original_width) <= width {
					beego.Info(fmt.Sprintf("Serving Original Image: %s | Size: %d X %d -> %4.f X %4.f", downloadUrl, original_width,original_height, width, height))
					http.ServeFile(this.Ctx.ResponseWriter, this.Ctx.Request, fileName)
					os.Remove(fileName)
					return
				}
				os.Remove(fileName)
				//Preserve aspect ratio
				width_ratio := width / float64(original_width)
				height_ratio := height / float64(original_height)
				if( width_ratio < height_ratio ) {
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
				beego.Info(fmt.Sprintf("Image: %s | Size: %d X %d -> %4.f X %4.f | Width Ration: %4.4f | Height Ratio: %4.4f | Quality: %d", downloadUrl, original_width,original_height, width, height, width_ratio, height_ratio, quality))
				resizedImage := resize.Resize(uint(width), uint(height), originalImg, resize.Lanczos3)
				resizeImageFile, err := os.Create(fileName)
				if err != nil {
					errMessage := fmt.Sprintf("Image Open Error: %s", err)
					beego.Error(errMessage)
					this.Ctx.Abort(500, errMessage)
					os.Remove(fileName)
					return
				}
				if fileExt == ".jpeg" || fileExt == ".jpg" {
					jpeg.Encode(resizeImageFile, resizedImage, &jpeg.Options{Quality: quality})
				}
				if fileExt == ".png" {
					png.Encode(resizeImageFile, resizedImage)
				}
				if fileExt == ".gif" {
					gif.Encode(resizeImageFile, resizedImage, &gif.Options{NumColors: 256})
				}
				resizeImageFile.Close()
				http.ServeFile(this.Ctx.ResponseWriter, this.Ctx.Request, fileName)
				os.Remove(fileName)
			}
		}
	} else {
		beego.Error("Error downloading file: " +downloadUrl +" Status: " +strconv.Itoa(response.StatusCode) +"[" +response.Status +"]")
		this.Ctx.Abort(response.StatusCode, response.Status)
		return
	}
}

func resizeUsingImageMagick(this *ResizeController, fileName string, width float64, height float64, quality int, downloadUrl string ) {
	//Serialize imagemagick calls
	imageMagickConcurrencyLock.Lock()
	defer imageMagickConcurrencyLock.Unlock()

	imagick.Initialize()
	defer imagick.Terminate()
	mw := imagick.NewMagickWand()
	defer mw.Destroy()
	err := mw.ReadImage(fileName)
	if err != nil {
		errMessage := fmt.Sprintf("Image Open Error: %s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		os.Remove(fileName)
		return
	}
	// Get original logo size
	original_width := mw.GetImageWidth()
	original_height := mw.GetImageHeight()
	width_ratio := width / float64(original_width)
	height_ratio := height / float64(original_height)
	if( width_ratio < height_ratio ) {
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
	beego.Info(fmt.Sprintf("Image: %s | Size: %d X %d -> %4.f X %4.f | Width Ration: %4.4f | Height Ratio: %4.4f | Quality: %d", downloadUrl, original_width,original_height, width, height, width_ratio, height_ratio, quality))
	// Resize the image using the Lanczos filter
	// The blur factor is a float, where > 1 is blurry, < 1 is sharp
	err = mw.ResizeImage(uint(width), uint(height), imagick.FILTER_LANCZOS, 1)
	if err != nil {
		errMessage := fmt.Sprintf("Image Resize Error: %s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		os.Remove(fileName)
		return
	}
	err = mw.SetImageCompressionQuality(uint(quality))
	if err != nil {
		errMessage := fmt.Sprintf("Image Quality Error: %s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		os.Remove(fileName)
		return
	}
	mw.SetImageInterlaceScheme(imagick.INTERLACE_PLANE)
	mw.SetImageFormat(strings.Replace(filepath.Ext(fileName), ".", "", -1))
	mw.WriteImage(fileName)
	http.ServeFile(this.Ctx.ResponseWriter, this.Ctx.Request, fileName)
	os.Remove(fileName)
}
