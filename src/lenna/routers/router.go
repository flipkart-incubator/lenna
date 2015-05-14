package routers

import (
	"github.com/astaxie/beego"
	"lenna/controllers"
)

func init() {
	beego.Router("/", &controllers.MainController{})
	beego.Router("/status", &controllers.StatusController{}, "get:StatusCheck")
	beego.Router("/oor", &controllers.StatusController{}, "post:OutOfRotation")
	beego.Router("/bir", &controllers.StatusController{}, "post:InRotation")
	beego.Router("/:what/:width:int/:height:int/:path(.*)", &controllers.ResizeController{}, "get:Get")
}
