package controllers

import (
	"github.com/astaxie/beego"
)

type StatusController struct {
	beego.Controller
}

var service_status bool = true

func (this *StatusController) StatusCheck() {
	if service_status == false {
		this.Ctx.Abort(503, "Service Unavailable")
	} else {
		this.Ctx.WriteString("Ok")
	}
}

func (this *StatusController) InRotation() {
	service_status = true
	this.Ctx.WriteString("Service In Rotation")
}

func (this *StatusController) OutOfRotation() {
	service_status = false
	this.Ctx.WriteString("Service Out Of Rotation")
}
