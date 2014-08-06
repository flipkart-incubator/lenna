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
	"github.com/nl5887/golang-image/jpeg"
)

type ResizeController struct {
	beego.Controller
}

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
	fileName := fmt.Sprintf("/tmp/%s.jpeg", u4)
	downloadUrl := fmt.Sprintf("%s%s", beego.AppConfig.String(what +".source"), imageUri)
	response, err := http.Get(downloadUrl)
	if err != nil {
		errMessage := fmt.Sprintf("%s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		return
	}
	if response.StatusCode != 200 {
		beego.Error("Error downloading file: " +downloadUrl +" Status: " +strconv.Itoa(response.StatusCode) +"[" +response.Status +"]")
		this.Ctx.Abort(response.StatusCode, response.Status)
		return
	}
	imageData, err := ioutil.ReadAll(response.Body)
	response.Body.Close()
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
	// decode jpeg into image.Image
	originalImg, err := jpeg.Decode(originalImageFile)
	if err != nil {
		errMessage := fmt.Sprintf("Image Decode Error: %s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		originalImageFile.Close()
		os.Remove(fileName)
		return
	}
	originalImageFile.Close()
	originalImageFile1, err := os.Open(fileName)
	imgc, _, err := image.DecodeConfig(originalImageFile1)
	if err != nil {
		errMessage := fmt.Sprintf("Image Get Decode Config Error: %s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		originalImageFile1.Close()
		os.Remove(fileName)
		return
	}
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
	jpeg.Encode(resizeImageFile, resizedImage, &jpeg.Options{Quality: quality})
	resizeImageFile.Close()
	http.ServeFile(this.Ctx.ResponseWriter, this.Ctx.Request, fileName)
	os.Remove(fileName)
}

