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

func (this *SpCmsConfig) Read(vertical string, verticalVersions []int, batchSize int, callback func(map[string]bool) int) (map[string]bool, bool) {

	var tBinaryProtocolFactory = thrift.NewTBinaryProtocolFactory(true, true)
	addr, err := net.ResolveTCPAddr("tcp", this.Host)
	if err != nil {
		fmt.Println("Failed|cmsConnectionOpen|Error resolving vertical", vertical, "address:", err, "\n")
		return map[string]bool{}, false
	}


	tTransportFactory := thrift.NewTTransportFactory()
	tSocket := thrift.NewTSocketAddr(addr)
	defer tSocket.Close()

	if !tSocket.IsOpen() {
		err = tSocket.Open()
		if err != nil {
			fmt.Println("Failed|cmsConnectionOpen| Error opening connection vertical", vertical," for protocol ", addr.Network(), " to ", addr.String(), ": ", err, "\n")
			return map[string]bool{}, false
		}
	}
	fmt.Println("Success|cmsConnectionOpen| Socket connection success", vertical)
	transport := tTransportFactory.GetTransport(tSocket)
	client := spcms.NewCMS_SP_ServiceClientFactory(transport, tBinaryProtocolFactory)

	if err != nil {
		fmt.Println("Failed|cmsConnectionOpen|Error resolving vertical", vertical, " address: %s", err, "\n")
		return map[string]bool{}, false
	}

	if len(verticalVersions) == 0 {
		verticalVersions, err = readVerticalVersion(vertical, client)

		if err != nil {
			return map[string]bool{}, false
		}
	}


	for idx := len(verticalVersions) - 1; idx >= 0; idx-- {
		version := verticalVersions[idx]
		productIds, err := readProductId(vertical, version, client, batchSize)
		if err != nil {
			return map[string]bool{}, false
		}

		urls, err := readUrls(vertical, version, productIds, client)
		if err != nil {
			return map[string]bool{}, false
		}
		fmt.Println("INFO|fetched|Vertical", vertical, " #URL=", len(urls))
		fmt.Println("Success|cmsConnectionClose|vertical=", vertical," Socket connection success")
		if callback != nil {
			written := callback(urls)
			fmt.Println("Success|callback|written", written, "vertical=", vertical," version", version)
		}

	}


	return nil, true
}

func readVerticalVersion(vertical string, client *spcms.CMS_SP_ServiceClient) ([]int, error) {
	cmsVerticalVersionsResult, cmsException, err := client.GetSPCmsVerticalVersions()
	if err != nil {
		fmt.Println("Failed|cmsGetVerticalVersion|vertical=", vertical, " Error while getting data from CMS", err)
		return []int{}, err
	}
	if cmsException != nil {
		fmt.Println("Failed|cmsGetVerticalVersion|vertical=", vertical, " Error while getting data from CMS", err)
		return []int{}, err
	}
	versionNumbers := cmsVerticalVersionsResult.VerticalVersionMap[vertical]
	versions := make([]int, versionNumbers)

	for idx := 0; idx < int (versionNumbers); idx++ {
		versions[idx] = idx
	}
	fmt.Println("Success|cmsGetVerticalVersion|vertical=", vertical, " Versions", versionNumbers)
	return versions, nil
}

func readUrls(vertical string, version int, pids []string, client *spcms.CMS_SP_ServiceClient) (map[string]bool, error) {

	productInfoRequest := spcms.NewProductInfoRequest()
	productInfoRequest.ProductIds = pids
	baseRequest := spcms.NewBaseRequest()
	productInfoRequest.BaseRequest = baseRequest
	r, cmsException, err := client.GetProductWithListings(productInfoRequest)

	if err != nil {
		fmt.Println("Failed|cmsGetUrl|Vertical", vertical, "version ", version, "Error while getting data from CMS", err)
		return map[string]bool{}, err
	}
	if cmsException != nil {
		fmt.Println("Failed|cmsGetUrl|Vertical", vertical, "version ", version, "Exception while getting data from CMS", cmsException)
		return map[string]bool{}, err
	}

	fmt.Println("Success|cmsGetUrl|#productId=", len(r.ProductDataWithListings), "Vertical", vertical, "version ", version)
	urls := make(map[string]bool)

	for _, val := range r.ProductDataWithListings {
		for _, v := range val.ProductData.StaticContentInfo {
			urls[v.TransContents[len(v.TransContents) - 1].AttributeValues["path"].ValuesList[0].Value] = true
		}
	}
	return urls, nil
}

func readProductId(vertical string, version int, client *spcms.CMS_SP_ServiceClient, batchSize int) ([]string, error) {
	request := cms.NewDeltaRequest()

	request.Vertical = vertical

	request.CurrentVersionNo = int64 (version)
	request.MaxResultSize = int32 (batchSize)
	deltaResult, cmsException, err := client.GetSPVerticalDelta(request)

	if err != nil {
		fmt.Println("Failed|cmsGetPID|vertical=", vertical, " version", version, "Error while getting data from CMS", err)
		return []string{}, err
	}
	if cmsException != nil {
		fmt.Println("Failed|cmsGetPID|vertical=", vertical, " version", version, "Exception while getting data from CMS", cmsException)
		return []string{}, err
	}
	fmt.Println("Success|cmsGetPID|vertical=", vertical, "version=", version)
	pids := []string{}
	for pid := range deltaResult.ProductIDs {
		pids = append(pids, (string) (deltaResult.ProductIDs[pid]))
	}
	fmt.Println("INFO|fetched|Vertical", vertical, "#PID=", len(pids))
	return pids, nil
}
