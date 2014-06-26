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
const host string = "sp-cms-service.nm.flipkart.com:26701"
const concurrency int = 5
const mqReadBulkSize int = 1


func main() {
	rabbitMQConfig := client.RabbitMQConfig{ AmqpConnection: "amqp://guest:guest@mobile-flipcast-queue1.nm.flipkart.com:5672/",
		QueueName :"rukmini_jobs"}
	amqpClientRead, err := rabbitMQConfig.CreateChannel(mqReadBulkSize, concurrency)
	amqpClientWrite, err := rabbitMQConfig.CreateChannel(mqReadBulkSize, concurrency)

	if err != nil {
		fmt.Println("exit 0")
		os.Exit(-1)
	}

	if err != nil {
		fmt.Println("exit 1")
		os.Exit(-1)
	}

//	callbackMethod := client.WarmUpCache(string)
	urls := [] string {
			"http://rukmini1.flixcart.com/image/500/500/television/u/e/9/samsung-23h4003-original-imadxd2fajkaswcu.jpeg?q=90",
			"http://rukmini1.flixcart.com/image/500/500/television/u/e/9/samsung-23h4003-original-imadxd2fajkaswcu.jpeg?q=80",
			"http://rukmini1.flixcart.com/image/500/500/television/u/e/9/samsung-23h4003-original-imadxd2fajkaswcu.jpeg?q=70",
			"http://rukmini1.flixcart.com/image/500/500/television/u/e/9/samsung-23h4003-original-imadxd2fajkaswcu.jpeg?q=60",
			"http://rukmini1.flixcart.com/image/500/500/television/u/e/9/samsung-23h4003-original-imadxd2fajkaswcu.jpeg?q=50" }
	written := amqpClientWrite.Write(urls)
	fmt.Println("written = ", written)
	amqpClientRead.Read()

	beego.SetLogger("file", `{"filename":"/var/log/rukmini/rukmini.log"}`)
	beego.Run()
}

