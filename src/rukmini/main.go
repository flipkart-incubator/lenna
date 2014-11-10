package main

import (
	_ "rukmini/routers"
	"github.com/astaxie/beego"
	"net/http"
)

func rukmini_error_handler(rw http.ResponseWriter, r *http.Request) {
	rw.WriteHeader(500)
	rw.Write([]byte("Image delivery error"))
}

func main() {
	beego.Errorhandler("500", rukmini_error_handler)
	beego.SetLogger("file", `{"filename":"/var/log/rukmini/rukmini.log"}`)
	beego.Run()
}

