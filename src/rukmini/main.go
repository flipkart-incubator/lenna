package main

import (
	_ "rukmini/routers"
	"github.com/astaxie/beego"
	"rukmini/client"
)

func main() {
	go client.Read("shoe")
	go client.Read("mobile")
	go client.Read("tablet")
	beego.SetLogger("file", `{"filename":"/var/log/rukmini/rukmini.log"}`)
	beego.Run()
}

