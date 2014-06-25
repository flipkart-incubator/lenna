package connector

import (
	"fmt"
	"thrift"
	"net"
	"github.com/thriftlib/spcms"
)

//TODO: The funtion name should be Read
func Test() error {
	//Create ThriftTransport and defer
//	var transport TTransport
//	defer transport.Close();

	//TODO: Move the hostname to Config
	addr, err := net.ResolveTCPAddr("tcp", "sp-cms-service.nm.flipkart.com:26701")
	if err != nil {
		fmt.Print("Error resolving address: %s", err, "\n")
		return err
	}

	tBinaryProtocolFactory := thrift.NewTBinaryProtocolFactoryDefault()
	transportFactory := thrift.NewTTransportFactory()
	transport := thrift.NewTSocketAddr(addr)
	defer transport.Close()

	if err = transport.Open(); err != nil {
		fmt.Print("Error opening connection for protocol ", addr.Network(), " to ", addr.String(), ": ", err, "\n")
		return err
	}
	useTransport := transportFactory.GetTransport(transport)

	client := spcms.NewCMS_SP_ServiceClientFactory(useTransport, tBinaryProtocolFactory)

	if err = transport.Open(); err != nil {
		fmt.Println("Error", err)
    }
	readProductId("SHODKG3HZ7NVGW49", transport, client)
	return nil
}

func readProductId(productId string, tSocket *thrift.TSocket, client *spcms.CMS_SP_ServiceClient) error {
	cmsVerticalVersionsResult, cmsException, err := client.GetSPVerticalVersion("shoe")
	if err != nil {
		fmt.Println("error while getting data from CMS", err)
	}
	if cmsException != nil {
		fmt.Println("Exception while getting data from CMS", cmsException)
	}
	fmt.Println("Success %s", cmsVerticalVersionsResult)
	return nil
}
