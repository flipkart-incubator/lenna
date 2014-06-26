package client

import (
	"fmt"
	"thrift"
	"net"
	"github.com/thriftlib/spcms"
)
//TODO: Use properties file instead of hardcoding
type SPCMS struct {
	host string
}

func (this *SPCMS) Read(vertical string) bool {
	var tBinaryProtocolFactory = thrift.NewTBinaryProtocolFactoryDefault()

	addr, err := net.ResolveTCPAddr("tcp", this.host)
	if err != nil {
		fmt.Println("Failed|cmsConnectionOpen|vertical=", vertical, " Error resolving address: %s", err, "\n")
		return false
	}

	var tTransportFactory = thrift.NewTTransportFactory()
	tSocket := thrift.NewTSocketAddr(addr)
	defer tSocket.Close()
	if !tSocket.IsOpen() {
		err = tSocket.Open()
		if err != nil {
			fmt.Println("Failed|cmsConnectionOpen|vertical=", vertical, " Error opening connection for protocol ", addr.Network(), " to ", addr.String(), ": ", err, "\n")
			return false
		}
	}
	fmt.Println("Success|cmsConnectionOpen|vertical=", vertical," Socket connection success")
	var transport = tTransportFactory.GetTransport(tSocket)
	client := spcms.NewCMS_SP_ServiceClientFactory(transport, tBinaryProtocolFactory)


	err = readProductId(vertical, client)
	if err != nil {
		return false
	}
	fmt.Println("Success|cmsConnectionClose|vertical=", vertical," Socket connection success")
	return true
}

func readProductId(vertical string, client *spcms.CMS_SP_ServiceClient) error {

	cmsVerticalVersionsResult, cmsException, err := client.GetSPVerticalVersion(vertical)
	if err != nil {
		fmt.Println("Failed|cmsGet|vertical=", vertical, " Error while getting data from CMS", err)
		return err
	}
	if cmsException != nil {
		fmt.Println("Failed|cmsGet|vertical=", vertical, " Exception while getting data from CMS", cmsException)
		return err
	}
	fmt.Println("Success|cmsGet|vertical=", vertical, "version=", cmsVerticalVersionsResult)
	return nil
}
