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
		}, func(err error) error {
			if err == hystrix.ErrTimeout {
				output <- nil
			}
			return hystrix.ErrTimeout
		})

	response = <- output
	endDownload := time.Now().UnixNano()
	if response == nil {
		err = <- errorChannel
		beego.Info("HttpCommand=Error StartTime=", startDownload, " EndTime=", endDownload, " URL=", input.URL, " Error=", err)
		return nil, err
	} else {
		beego.Info("HttpCommand=Success StartTime=", startDownload, " EndTime=", endDownload, " URL=", input.URL)
		return response, nil
	}

}
