package controllers

import (
	"fmt"
	"io/ioutil"
	"os"
	"net/http"
	"strconv"
	"github.com/astaxie/beego"
	"github.com/satori/uuid"
	"github.com/gographics/imagick/imagick"
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
	imagick.Initialize()
	//Create new magickwand canvas
	mw := imagick.NewMagickWand()
	//Read the image into memory
	err = mw.ReadImage(fileName)
	if err != nil {
		errMessage := fmt.Sprintf("Image Read Error: %s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		mw.Destroy()
		imagick.Terminate()
		return
	}
	var original_width = mw.GetImageWidth()
	var original_height = mw.GetImageHeight()
	beego.Info(fmt.Sprintf("Image: %s | Size: %d X %d -> %4.f X %4.f", downloadUrl, original_height,original_width, height, width))
	if float64(original_height) <= height || float64(original_width) <= width {
		http.ServeFile(this.Ctx.ResponseWriter, this.Ctx.Request, fileName)
		mw.Destroy()
		imagick.Terminate()
		return
	}
	//Preserve aspect ratio
	if uint(width) > original_width {
		ratio := float64(original_width / uint(width))
		width = float64(width * ratio)
		height = float64(height * ratio)
	}
	if uint(height) > original_height {
		ratio := float64(original_height / uint(height))
		width = float64(width * ratio)
		height = float64(height * ratio)
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
	err = mw.ResizeImage(uint(width), uint(height), imagick.FILTER_LANCZOS, 0.8)
	if err != nil {
		errMessage := fmt.Sprintf("Image Resize Error: %s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		mw.Destroy()
		imagick.Terminate()
		os.Remove(fileName)
		return
	}
	err = mw.SetImageCompressionQuality((uint)(quality))
	if err != nil {
		errMessage := fmt.Sprintf("Image Quality Setting Error: %s", err)
		beego.Error(errMessage)
	}
	err = mw.SetImageInterlaceScheme(imagick.INTERLACE_PLANE)
	if err != nil {
		errMessage := fmt.Sprintf("Progressive Rendering Error: %s", err)
		beego.Error(errMessage)
	}
	mw.SetImageFormat("jpeg")
	err = mw.WriteImage(fileName);
	if err != nil {
		errMessage := fmt.Sprintf("Image Write Error: %s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		mw.Destroy()
		imagick.Terminate()
		os.Remove(fileName)
		return
	}
	mw.Destroy()
	imagick.Terminate()
	http.ServeFile(this.Ctx.ResponseWriter, this.Ctx.Request, fileName)
	os.Remove(fileName)
}

