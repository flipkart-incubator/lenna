package main

import (
	"github.com/astaxie/beego"
	"net/http"
	_ "rukmini/routers"
	"github.com/afex/hystrix-go/hystrix"
	"net"
)

func rukmini_error_handler(rw http.ResponseWriter, r *http.Request) {
	rw.Write([]byte("Image delivery error"))
}

func rukmini_bad_request_handler(rw http.ResponseWriter, r *http.Request) {
	rw.Write([]byte("Bad request"))
}

func main() {

	initErrorHandler()
	initLogger()
	initHystrixDashboard()
	initHystrixCommand()
	beego.Run()
}

func initHystrixCommand() {
	fk_cdn_timeout, _ := beego.AppConfig.Int("fkcdn_timeout");
	hystrix.ConfigureCommand("fk-cdn", hystrix.CommandConfig{
		Timeout:               fk_cdn_timeout,
		MaxConcurrentRequests: 100,
		ErrorPercentThreshold: 90,
	})
}

func initHystrixDashboard() {
	hystrixStreamHandler := hystrix.NewStreamHandler()
	hystrixStreamHandler.Start()
	go http.ListenAndServe(net.JoinHostPort("", "8182"), hystrixStreamHandler)
}

func initLogger() {
	beego.SetLogger("file", `{"filename":"/var/log/rukmini/rukmini.log", "daily" : true, "maxdays": 3, "rotate" : true}`)
}

func initErrorHandler() {
	beego.Errorhandler("500", rukmini_error_handler)
	beego.Errorhandler("400", rukmini_bad_request_handler)
}
