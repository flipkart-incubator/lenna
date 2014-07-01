package main

import (
	"flag"
	"os"
	"fmt"
	"rukmini/client"
	"strings"
	"strconv"
)

const amqpConnection string = "amqp://guest:guest@mobile-flipcast-queue1.nm.flipkart.com:5672/"
const queueName string = "rukmini_jobs"
const concurrency int = 5
const mqReadBulkSize int = 1


func main() {
	batchSize := flag.Int("batchSize", 10, "batch size to read from CMS")
	host := flag.String("cmsHost", "sp-cms-search-service.vip.nm.flipkart.com:26701", "CMS Host end point <host>:<port>")
	vertical := flag.String("vertical", "", "vertical for which images are to be populated")
	versionArg := flag.String("versions", "", "version for which images are to be populated. Versions should be comma separated")

	flag.Parse()

	if *vertical == "" {
		fmt.Println("No vertical name provided")
		os.Exit(-1)
	}

	version := []int{}
	if *versionArg != "" {
		versionStringArray := strings.Split(*versionArg, ",")
		version = make([]int, len(versionStringArray))
		for idx := 0; idx < len(versionStringArray); idx++ {
			integer, err := strconv.Atoi(versionStringArray[idx])
			if err != nil {
				fmt.Println("Unable to parse argument versions")
				os.Exit(-1)
			}
			version[idx] = integer
		}
	}

	spCMSConfig := client.SpCmsConfig{Host : *host}

	rabbitMQConfig := client.RabbitMQConfig{ AmqpConnection: amqpConnection, QueueName :queueName}
	amqpClientWrite, err := rabbitMQConfig.CreateChannel(mqReadBulkSize, concurrency)

	if err != nil {
		fmt.Println("unable to create rabbitMqChannel")
		os.Exit(-1)
	}

	callback := amqpClientWrite.Write

	written, status := spCMSConfig.Read(*vertical, version, *batchSize, callback)

	if !status {
		fmt.Println("Failed!")
	}

	if written != nil {
		fmt.Println("INFO|pushed|#URL=", written)
	}

}
