package main

import (
	_ "rukmini/routers"
	"github.com/astaxie/beego"
	"rukmini/client"
	"os"
	"fmt"
)


//TODO: Move this to Properties file
const amqpConnection string = "amqp://guest:guest@mobile-flipcast-queue1.nm.flipkart.com:5672/"
const queueName string = "rukmini_jobs"
const host string = "sp-cms-service10.nm.flipkart.com:26701"
const concurrency int = 5
const mqReadBulkSize int = 1


func main() {
	spCMSConfig := client.SpCmsConfig{Host : host}
	fmt.Println("Client created")
	urls, status := spCMSConfig.Read("mobile")
	if !status {
		fmt.Println("Failed!")
	}

	rabbitMQConfig := client.RabbitMQConfig{ AmqpConnection: amqpConnection, QueueName :queueName}
	amqpClientRead, err := rabbitMQConfig.CreateChannel(mqReadBulkSize, concurrency)
	amqpClientWrite, err := rabbitMQConfig.CreateChannel(mqReadBulkSize, concurrency)

	if err != nil {
		fmt.Println("exit 1")
		os.Exit(-1)
	}

	written := amqpClientWrite.Write(urls)
	fmt.Println("INFO|pushed|#URL=", written)
	amqpClientRead.Read()

	beego.SetLogger("file", `{"filename":"/var/log/rukmini/rukmini.log"}`)
	beego.Run()
}

