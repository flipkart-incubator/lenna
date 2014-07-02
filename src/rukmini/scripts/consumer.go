package main


import (
	"fmt"
	"rukmini/client"
	"os"
	"flag"
)

const amqpConnection string = "amqp://guest:guest@mobile-flipcast-queue1.nm.flipkart.com:5672/"
const queueName string = "rukmini_jobs"
const queueNameSideline string = "rukmini_jobs_sideline"
const concurrency int = 1

func main() {
	batchSize := flag.Int("batchSize", 1, "batch size to read from Queue")
	host := flag.String("RukminiHost", "rukmini.flixcart.vip.nm.flipkart.com", "Rukmini Host")

	flag.Parse()

	rabbitMQConfig := client.RabbitMQConfig{ AmqpConnection: amqpConnection, QueueName: queueName, SideLineQueueName: queueNameSideline, BatchSize: *batchSize}
	rukminiConfig := client.RukminiConfig{ Host: *host }
	amqpClientRead, err := rabbitMQConfig.CreateChannel()


	if err != nil {
		fmt.Println("exit 1")
		os.Exit(-1)
	}

	callback := rukminiConfig.WarmUpCache
	amqpClientRead.Read(callback)
}

