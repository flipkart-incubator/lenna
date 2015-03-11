package command

import (
	"net/http"
	"github.com/afex/hystrix-go/hystrix"
	"github.com/astaxie/beego"
	"time"
)

type HttpCommand struct {
	client *http.Client
	hystrixCommand string
}

func Init(c *http.Client, h string) (*HttpCommand) {
	return &HttpCommand{
		client: c,
		hystrixCommand: h,
	}
}

func (this HttpCommand) Execute(input *http.Request) (response *http.Response, err error){
	output := make(chan *http.Response, 1)
	startDownload := time.Now().UnixNano()
	errorChannel := hystrix.Go(this.hystrixCommand, func() error {
			response, err = this.client.Do(input)
			output <- response
			return err
		}, nil)

	response = <- output
	endDownload := time.Now().UnixNano()
	if len(errorChannel) > 0 {
		beego.Info("Error Download time ", endDownload - startDownload)
		return nil, <- errorChannel
	} else {
		beego.Info("Download time ", endDownload - startDownload)
		return response, nil
	}

}
