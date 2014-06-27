package client

import (
	"github.com/streadway/amqp"
	"fmt"
	"strings"
)

type RabbitMQConfig struct {
	AmqpConnection string
	QueueName      string
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
	//TODO: Check more about Qos
	//	ch.Qos(bulkSize, concurrency * bulkSize, false)

	rabbitMQClient := RabbitMQClient{Channel: ch, Config: this}
	return &rabbitMQClient, nil
}

func (this *RabbitMQClient) Write(urls map[string]bool) int {

	//Publish all url to a queue
	index := 0
	for key, _ := range urls {
		index++
		url := constructRukminiUrl(key)
		err := this.Channel.Publish(
			"",           // exchange
			this.Config.QueueName, // routing key
			false,        // mandatory
			false,
			amqp.Publishing {
			DeliveryMode:  amqp.Persistent,
			ContentType:     "text/plain",
			Body:            []byte(url),
		})
		if err != nil {
			fmt.Println(err, "Failure|amqpConnectionOpen|URLS=", key, " Failed to publish a message")
			return index
		}
	}
	return len(urls)
}

func constructRukminiUrl(url string) string {
	newUrl := strings.SplitN(url, "/", 3)
	stringArray := []string{"http:/", "{host}", newUrl[1], "{width}/{height}", newUrl[2]}
	return strings.Join(stringArray, "/")
}

func (this *RabbitMQClient) Read(callback func(string) bool) bool {

	resp, err := this.Channel.Consume(this.Config.QueueName, "", false, false, false, false, nil)
	<-resp
	if err != nil {
		fmt.Println(err, "Failure|amqpConnection|Unable to consume from RabbitMQ")
		return false
	}

	this.process(resp, callback)
	return true;
}

func (this *RabbitMQClient) process(resp <-chan amqp.Delivery, callback func(string) bool) {

	for eachMessage := range resp {
		go func() {
			fmt.Println("Success|amqpPop|URL=", string(eachMessage.Body), " Pop successful")

			if callback != nil {
				done := callback(string(eachMessage.Body))
				if done {
					this.Channel.Ack(eachMessage.DeliveryTag, false)
				}
			} else {
				this.Channel.Ack(eachMessage.DeliveryTag, false)
			}
		}()
	}

}
