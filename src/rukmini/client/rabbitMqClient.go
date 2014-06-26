package client

import (
	"github.com/streadway/amqp"
	"fmt"
)

type RabbitMQConfig struct {
	AmqpConnection string
	QueueName string
}

type RabbitMQClient struct {
	Channel *amqp.Channel
	Config *RabbitMQConfig
}

func (this *RabbitMQConfig) CreateChannel(bulkSize int, concurrency int) (*RabbitMQClient, error) {
	//Create connection with rabbitMQ
	conn, err := amqp.Dial(this.AmqpConnection)
	if err != nil {
		fmt.Println(err, "Failure|amqpConnectionOpen| Failed to connect to RabbitMQ")
		return nil, err
	}

	//Create channel
	ch, err := conn.Channel()
	if err != nil {
		fmt.Println(err, "Failure|amqpConnectionOpen| Failed to open a channel")
		return nil, err
	}
	ch.Qos(bulkSize, concurrency * bulkSize, false)

	rabbitMQClient := RabbitMQClient{Channel: ch, Config: this}
	return &rabbitMQClient, nil
}

func (this *RabbitMQClient) Write(urls []string) int {

	//Publish all url to a queue
	for i := 0; i < len(urls); i++ {
		fmt.Println(urls[i])
		err := this.Channel.Publish(
			"",           // exchange
			this.Config.QueueName, // routing key
			false,        // mandatory
			false,
			amqp.Publishing {
				DeliveryMode:  amqp.Persistent,
				ContentType:     "text/plain",
				Body:            []byte(urls[i]),
		})
		if err != nil {
			fmt.Println(err, "Failure|amqpConnectionOpen|URLS=", urls[i], " Failed to publish a message")
			return i
		}
	}
	return len(urls)
}

func (this *RabbitMQClient) Read() bool {

	resp, err := this.Channel.Consume(this.Config.QueueName, "", false, false, false, false, nil)
	if err != nil {
		fmt.Println(err, "Failure|amqpConnection|Unable to consume from RabbitMQ")
		return false
	}
	done := make(chan bool)
	go func() {
		for eachMessage := range resp {
			fmt.Println("Success|amqpPop|URL=", eachMessage.Body, " Pop successful")

			done := WarmUpCache(string(eachMessage.Body))
			if done {
				this.Channel.Ack(eachMessage.DeliveryTag, false)
			}
		}
	}()
	<-done
	return true;
}
