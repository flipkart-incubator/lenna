package main

import (
	"flag"
	"github.com/afex/hystrix-go/hystrix"
	"github.com/astaxie/beego"
	"lenna/conf"
	_ "lenna/routers"
	"net"
	"net/http"
	"os"
	"runtime"
	"runtime/pprof"
)

var cpuprofile = flag.String("cpuprofile", "", "write cpu profile to file")
var memprofile = flag.String("memprofile", "", "write memory profile to file")

func rukmini_error_handler(rw http.ResponseWriter, r *http.Request) {
	rw.Write([]byte("Image delivery error"))
}

func rukmini_bad_request_handler(rw http.ResponseWriter, r *http.Request) {
	rw.Write([]byte("Bad request"))
}

func main() {
	//GOMAXPROCS is being set by beego and nfnt.resize. But just to be sure this variable is set
	runtime.GOMAXPROCS(runtime.NumCPU())
	flag.Parse()

	initErrorHandler()
	initLogger()
	initHystrixDashboard()
	initHystrixCommand()
	if isRunModeDebug() {
		setDebugFlags()
	}

	beego.Run()
}

func setDebugFlags() {
	if *cpuprofile != "" {
		f, err := os.Create(*cpuprofile)
		if err != nil {
			beego.Error(err)
		}
		pprof.StartCPUProfile(f)
		defer pprof.StopCPUProfile()
		beego.Info("profiling cpu")
	} else {
		beego.Info("not profiling cpu")
	}

	if *memprofile != "" {
		f, err := os.Create(*memprofile)
		if err != nil {
			beego.Error(err)
		}
		pprof.WriteHeapProfile(f)
		defer pprof.StopCPUProfile()
		beego.Info("profiling memory")
	} else {
		beego.Info("not profiling memory")
	}

}

func isRunModeDebug() bool {
	return *cpuprofile != "" || *memprofile != ""
}

func initHystrixCommand() {
	hystrix.ConfigureCommand(conf.FK_CDN_HYSTRIX_COMMAND, hystrix.CommandConfig{
		Timeout:               int(conf.GetFkCdnTimeout()),
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
	beego.SetLogger("file", `{"filename":"/var/log/flipkart/lenna.log", "daily" : true, "maxdays": 3, "rotate" : true}`)
}

func initErrorHandler() {
	beego.Errorhandler("500", rukmini_error_handler)
	beego.Errorhandler("400", rukmini_bad_request_handler)
}
