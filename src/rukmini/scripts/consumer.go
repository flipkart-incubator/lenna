package main


import (
	"fmt"
	"rukmini/client"
	"os"
	"flag"
)

const amqpConnection string = "amqp://guest:guest@mobile-flipcast-queue1.nm.flipkart.com:5672/"
const queueName string = "rukmini_jobs"
const concurrency int = 1

func main() {
	host := flag.String("RukminiHost", "rukmini.flixcart.com", "rukmini host which is to be called")
	batchSize := flag.Int("batchSize", 1000, "batch size to read from Queue")

	rabbitMQConfig := client.RabbitMQConfig{ AmqpConnection: amqpConnection, QueueName :queueName}
	rukminiConfig := client.RukminiConfig{ Host : *host}
	amqpClientRead, err := rabbitMQConfig.CreateChannel(*batchSize, concurrency)


	if err != nil {
		fmt.Println("exit 1")
		os.Exit(-1)
	}

	callback := rukminiConfig.WarmUpCache
	amqpClientRead.Read(callback)
}
