package client

import (
	"fmt"
	"thrift"
	"net"
	"github.com/thriftlib/spcms"
	"github.com/thriftlib/cms"
)

type SpCmsConfig struct {
	Host string
}

func (this *SpCmsConfig) Read(vertical string) (map[string]bool, bool) {

	var tBinaryProtocolFactory2 = thrift.NewTBinaryProtocolFactory(true, true)
	addr, err := net.ResolveTCPAddr("tcp", this.Host)
	if err != nil {
		fmt.Println("Failed|cmsConnectionOpen|Error resolving address: %s", err, "\n")
		return map[string]bool{}, false
	}


	tTransportFactory := thrift.NewTTransportFactory()
	tSocket := thrift.NewTSocketAddr(addr)

	if !tSocket.IsOpen() {
		err = tSocket.Open()
		if err != nil {
			fmt.Println("Failed|cmsConnectionOpen| Error opening connection for protocol ", addr.Network(), " to ", addr.String(), ": ", err, "\n")
			return map[string]bool{}, false
		}
	}
	fmt.Println("Success|cmsConnectionOpen| Socket connection success")
	transport := tTransportFactory.GetTransport(tSocket)
	client2 := spcms.NewCMS_SP_ServiceClientFactory(transport, tBinaryProtocolFactory2)


	if err != nil {
		fmt.Println("Failed|cmsConnectionOpen|Error resolving address: %s", err, "\n")
		return map[string]bool{}, false
	}

	productIds, err := readProductId(vertical, client2)
	if err != nil {
		return map[string]bool{}, false
	}
	fmt.Println("INFO|fetched|#PID=", len(productIds))
	urls, err := readUrls(productIds, client2)
	if err != nil {
		return map[string]bool{}, false
	}
	fmt.Println("INFO|fetched|#URL=", len(urls))
	fmt.Println("Success|cmsConnectionClose|vertical=", vertical," Socket connection success")
	return urls, true
}

func readUrls(pids []string, client *spcms.CMS_SP_ServiceClient) (map[string]bool, error) {

	productInfoRequest := spcms.NewProductInfoRequest()
	productInfoRequest.ProductIds = pids
	baseRequest := spcms.NewBaseRequest()
	productInfoRequest.BaseRequest = baseRequest
	r, cmsException, err := client.GetProductWithListings(productInfoRequest)

	if err != nil {
		fmt.Println("Failed|cmsGet| Error while getting data from CMS", err)
		return map[string]bool{}, err
	}
	if cmsException != nil {
		fmt.Println("Failed|cmsGet| Exception while getting data from CMS", cmsException)
		return map[string]bool{}, err
	}

	fmt.Println("Success|cmsGet|productId=", r)
	urls := make(map[string]bool)

	for _, val := range r.ProductDataWithListings {
		for _, v := range val.ProductData.StaticContentInfo {
			urls[v.TransContents[len(v.TransContents) - 1].AttributeValues["path"].ValuesList[0].Value] = true
		}
	}
	fmt.Println(len(urls))
	return urls, nil
}

func readProductId(vertical string, client *spcms.CMS_SP_ServiceClient) ([]string, error) {
	request := cms.NewDeltaRequest()

	request.Vertical = vertical
	cmsVerticalVersionsResult, _, _ := client.GetSPCmsVerticalVersions()
	fmt.Println(cmsVerticalVersionsResult)
	versionNumber := cmsVerticalVersionsResult.VerticalVersionMap[vertical]
	request.CurrentVersionNo = versionNumber - 10
	request.MaxResultSize = 10
	deltaResult, cmsException, err := client.GetSPVerticalDelta(request)

	if err != nil {
		fmt.Println("Failed|cmsGet|vertical=", vertical, " Error while getting data from CMS", err)
		return []string{}, err
	}
	if cmsException != nil {
		fmt.Println("Failed|cmsGet|vertical=", vertical, " Exception while getting data from CMS", cmsException)
		return []string{}, err
	}
	fmt.Println("Success|cmsGet|vertical=", vertical, "version=", versionNumber)
	pids := []string{}
	for pid := range deltaResult.ProductIDs {
		pids = append(pids, (string) (deltaResult.ProductIDs[pid]))
	}

	return pids, nil
}
