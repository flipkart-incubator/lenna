package main

import (
	_ "rukmini/routers"
	"github.com/astaxie/beego"
	"os"
	"os/signal"
	"syscall"
	"fmt"
)

func main() {

	sigc := make(chan os.Signal, 1)
	signal.Notify(sigc, syscall.SIGHUP, syscall.SIGINT, syscall.SIGABRT, syscall.SIGQUIT, syscall.SIGILL,
		syscall.SIGFPE, syscall.SIGKILL, syscall.SIGSEGV, syscall.SIGPIPE, syscall.SIGALRM, syscall.SIGTERM, syscall.SIGUSR1,
		syscall.SIGUSR2, syscall.SIGCHLD, syscall.SIGCONT, syscall.SIGSTOP, syscall.SIGTSTP, syscall.SIGTTIN, syscall.SIGTTOU)
	go func() {
		s := <- sigc
		beego.Error("SIGNAL: " +s.String())
		fmt.Printf("SIGNAL: " +s.String())
	}()
	beego.SetLogger("file", `{"filename":"/var/log/rukmini/rukmini.log"}`)
	beego.Run()
}

