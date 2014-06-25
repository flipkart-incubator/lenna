package main

import (
	_ "rukmini/routers"
	"github.com/astaxie/beego"
	"rukmini/connector"
)

func main() {
	connector.Test()
	beego.SetLogger("file", `{"filename":"/var/log/rukmini/rukmini.log"}`)
	beego.Run()
}

