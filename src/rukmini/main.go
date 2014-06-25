package main

import (
	_ "rukmini/routers"
	"github.com/astaxie/beego"
	"rukmini/client"
)

func main() {
	go client.Write([]string{"b", "c"})
	go client.Write([]string{"1", "2"})
	go client.Write([]string{"10", "100"})
	go client.Write([]string{"xx", "xxx"})
	go client.Read("shoe")
	go client.Read("mobile")
	go client.Read("tablet")
	beego.SetLogger("file", `{"filename":"/var/log/rukmini/rukmini.log"}`)
	beego.Run()
}

