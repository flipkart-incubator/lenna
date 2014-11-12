package main

import (
	"github.com/astaxie/beego"
	"net/http"
	_ "rukmini/routers"
)

func rukmini_error_handler(rw http.ResponseWriter, r *http.Request) {
	rw.Write([]byte("Image delivery error"))
}

func rukmini_bad_request_handler(rw http.ResponseWriter, r *http.Request) {
	rw.Write([]byte("Bad request"))
}

func main() {
	beego.Errorhandler("500", rukmini_error_handler)
	beego.Errorhandler("400", rukmini_bad_request_handler)
	beego.SetLogger("file", `{"filename":"/var/log/rukmini/rukmini.log", "daily" : true, "maxdays": 3, "rotate" : true, "maxsize" : 1000000000}`)
	beego.Run()
}
