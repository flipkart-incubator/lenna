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
	width, err := strconv.ParseFloat(this.Ctx.Input.Param(":width"), 64)
	height, err := strconv.ParseFloat(this.Ctx.Input.Param(":height"), 64)
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
	downloadUrl := fmt.Sprintf("%s%s", beego.AppConfig.String("cdn"), imageUri)
	response, err := http.Get(downloadUrl)
	if err != nil {
		errMessage := fmt.Sprintf("%s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		return
	}
	defer response.Body.Close()
	imageData, err := ioutil.ReadAll(response.Body)
	if err = ioutil.WriteFile(fileName, imageData, os.ModePerm); err != nil {
		errMessage := fmt.Sprintf("%s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		return
	}
	imagick.Initialize()
	// Schedule cleanup
	defer imagick.Terminate()
	//Create new magickwand canvas
	mw := imagick.NewMagickWand()
	// Schedule cleanup
	defer mw.Destroy()
	//Read the image into memory
	err = mw.ReadImage(fileName)
	if err != nil {
		errMessage := fmt.Sprintf("Image Read Error: %s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		return
	}
	var original_width = mw.GetImageWidth()
	var original_height = mw.GetImageHeight()
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
		return
	}
	err = mw.SetImageCompressionQuality((uint)(quality))
	if err != nil {
		errMessage := fmt.Sprintf("Image Quality Setting Error: %s", err)
		beego.Error(errMessage)
	}
	mw.SetImageFormat("jpeg")
	err = mw.WriteImage(fileName);
	if err != nil {
		errMessage := fmt.Sprintf("Image Write Error: %s", err)
		beego.Error(errMessage)
		this.Ctx.Abort(500, errMessage)
		return
	}
	defer os.Remove(fileName)
	http.ServeFile(this.Ctx.ResponseWriter, this.Ctx.Request, fileName)
}
