package client

import (
	"github.com/streadway/amqp"
	"fmt"
)

func Write(urls []string) {
	conn, err := amqp.Dial("amqp://guest:guest@mobile-flipcast-queue1.nm.flipkart.com:5672/")
	onError(err, "Failed to connect to RabbitMQ")
	defer conn.Close()

	ch, err := conn.Channel()
	onError(err, "Failed to open a channel")
	defer ch.Close()

	for i := 0; i < len(urls); i++ {
		fmt.Println(urls[i])
		err = ch.Publish(
			"",           // exchange
			"rukmini_jobs", // routing key
			false,        // mandatory
			false,
			amqp.Publishing {
				DeliveryMode:  amqp.Persistent,
				ContentType:     "text/plain",
				Body:            []byte(urls[i]),
			})
		onError(err, "Failed to publish a message")
	}

}

func onError(err error, message string) {
	if err != nil {
		fmt.Println(message, err)
	}
}
