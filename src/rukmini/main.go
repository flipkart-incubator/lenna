package main

import (
	_ "rukmini/routers"
	"github.com/astaxie/beego"
)

func main() {
	beego.SetLogger("file", `{"filename":"/var/log/rukmini/rukmini.log"}`)
	beego.Run()
}

