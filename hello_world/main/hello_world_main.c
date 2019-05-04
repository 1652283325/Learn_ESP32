/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
//#include "freertos/semphr.h"
//#include "freertos/ringbuf.h"
//#include "freertos/projdefs.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_smartconfig.h"

//#include "esp_spi_flash.h"


#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#define WEB_SERVER "api.nbcgzn.com"
#define ESP_CONNECT_MAXIMUM_RETRY 5

static const char* TAG = "hello_world";

struct sockinfo{
	int 		sock;
	sa_family_t	sa_familyType;
	char * 		remoteIp;
	u16_t		remotePort;
};

//static char testItem[] = "The test string,test Ringbuffer split or no split or RINGBUF_TYPE_BYTEBUF";
//static const char* TAG 	= "TestLOG";
//static const char* TAG1 = "SendTask";
//RingbufHandle_t  buf_Handle;

//�����ź������ �ź������ ������ֵ����ʼ������ֵ �������ź��������Ƿ�����Դ���á���
//MAX_COUNT ��������������м�����Դ
xSemaphoreHandle CountHandle;
#define MAX_SOC_COUNT	2
#define INIT_SOC_COUNT	2

static void test_task(void* args);
#define ESP_TEST_MAIN_STACK	2048
#define ESP_TEST_MAIN_PRIO	5

//static void UDP_RecvTask(void * args);
//#define ESP_UDP_RECV_STACK	2048
//#define ESP_UDP_RECV_PRIO	6

static void Tcp_Client1(void *args);
#define ESP_TCP_CLIENT_STACK	2048
#define ESP_TCP_CLIENT_PRIO		7

static void Tcp_Client2(void *args);
#define ESP_TCP_CLIENT2_STACK	2048
#define ESP_TCP_CLIENT2_PRIO	6
//static void Send_Ringbuffer(void * args);
//#define ESP_SEND_MAIN_STACK	2048
//#define ESP_SEND_MAIN_PRIO	16
//��һ�����鱣����������ָ��
TaskFunction_t taskList[MAX_SOC_COUNT] = {Tcp_Client1,Tcp_Client2};//

//WIFI�¼���־�� Ҳ�������������б�־�飨���񱻴�����set ��ɾ����clean �൱����Դ�嵥��Щ��Դ�ǿ��õģ�
static EventGroupHandle_t wifi_EventHandle = NULL;
const int CONNECTED_BIT = BIT0;
const int TASK1_BIT		= BIT1;
const int TASK2_BIT		= BIT2;
//const int SCANDONE_BIT	= BIT1;

static int s_retryNum = 0;

static uint8_t previousSock;
//static const char *payload = "Message from ESP32 ";
//const wifi_scan_config_t scanCfg = {
//	.ssid 		= NULL,
//	.bssid		= NULL,
//	.channel 	=0,
//	.show_hidden=1,
//	.scan_type	=WIFI_SCAN_TYPE_ACTIVE,
//	.scan_time.active = {
//			.min = 0,
//		    .max = 1000
//	}
//};

void Smart_cfg_cb(smartconfig_status_t status, void *pdata)
{
	switch (status) {
	        case SC_STATUS_WAIT:
	            ESP_LOGI(TAG, "SC_STATUS_WAIT");
	            break;
	        case SC_STATUS_FIND_CHANNEL:
	            ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
	            break;
	        case SC_STATUS_GETTING_SSID_PSWD:
	            ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
	            break;
	        case SC_STATUS_LINK:
	            ESP_LOGI(TAG, "SC_STATUS_LINK");
	            wifi_config_t *wifi_config = pdata;
	            ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
	            ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
	            ESP_ERROR_CHECK( esp_wifi_disconnect() );
	            ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
	            ESP_ERROR_CHECK( esp_wifi_connect() );
	            break;
	        case SC_STATUS_LINK_OVER:
	            ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
	            if (pdata != NULL) {
	                uint8_t phone_ip[4] = { 0 };
	                memcpy(phone_ip, (uint8_t* )pdata, 4);
	                ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
	            }
	            break;
	        default:
	            break;
	    }
}
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
//	uint16_t scanNum = 0;
//	char  buff[53];
	wifi_config_t wifi_cfg;
	switch(event->event_id){
	case SYSTEM_EVENT_STA_START:
		esp_wifi_get_config(ESP_IF_WIFI_STA,&wifi_cfg);
		if(strlen((const char *)wifi_cfg.sta.ssid) != 0){
			ESP_LOGW(TAG,"Try to connect target AP ssid:%s  password : %s",wifi_cfg.sta.ssid,wifi_cfg.sta.password);
			ESP_ERROR_CHECK(esp_wifi_connect());
		}else ESP_LOGW(TAG,"SSID is NULL");
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		s_retryNum = 0;
		ESP_LOGI(TAG,"GOT IP :%s",ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
//		int err = getaddrinfo(WEB_SERVER, "80", &hints, &res);
		xEventGroupSetBits(wifi_EventHandle,CONNECTED_BIT|TASK1_BIT|TASK2_BIT);
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		switch(event->event_info.disconnected.reason){
		case WIFI_REASON_NO_AP_FOUND :
			ESP_LOGW(TAG,"Can't find target AP!");
			break;
		case WIFI_REASON_NOT_ASSOCED :
			ESP_LOGW(TAG,"Wrong password!!");
			break;
		default :
			ESP_LOGW(TAG,"Disconnect by other reasons error code:%d",event->event_info.disconnected.reason);
			break;
		}
		if(s_retryNum < ESP_CONNECT_MAXIMUM_RETRY){
			xEventGroupClearBits(wifi_EventHandle,CONNECTED_BIT);

			esp_wifi_connect();
			s_retryNum++;
			esp_wifi_get_config(ESP_IF_WIFI_STA,&wifi_cfg);
			ESP_LOGW(TAG,"Retry to connect target AP ssid:%s  password : %s",wifi_cfg.sta.ssid,wifi_cfg.sta.password);

		}else{
			ESP_LOGE(TAG,"Connect to AP fail! Start smartconfig\n");
			ESP_ERROR_CHECK(esp_smartconfig_start(Smart_cfg_cb,1));
		}
		break;
	case SYSTEM_EVENT_SCAN_DONE:
		ESP_LOGI(TAG,"SYSTEM_EVENT_SCAN_DONE");
//		ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&scanNum));
//		//ΪscanList������Ӧ��С���ڴ�
//		wifi_ap_record_t* scanList = (wifi_ap_record_t*)heap_caps_malloc(sizeof(wifi_ap_record_t)*scanNum, MALLOC_CAP_8BIT);
//		if(scanList != NULL){
//			ESP_LOGW(TAG,"Memory allocate success!!");
//			ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&scanNum,scanList));
//			if(scanNum == 0){
//				ESP_LOGI(TAG,"No ap has been found!");
//			}else{
//				ESP_LOGI(TAG,"%d APS found",scanNum);
//				for(int i = 0;i < scanNum;i++){
//					strcpy(buff,(const char *)scanList[i].ssid);
//					strcat(buff,"--------------------");
//					char * authMode;
//					switch(scanList[i].authmode){
//					case WIFI_AUTH_OPEN:
//						authMode = "OPEN\t\t";
//						break;
//					case WIFI_AUTH_WEP:
//						authMode = "WEP\t\t";
//						break;
//					case WIFI_AUTH_WPA_PSK:
//						authMode = "WPA_PSK\t";
//						break;
//					case WIFI_AUTH_WPA2_PSK:
//						authMode = "WPA2_PSK\t";
//						break;
//					case WIFI_AUTH_WPA_WPA2_PSK:
//					authMode = "WPA_WPA2_PSK\t";
//						break;
//					case WIFI_AUTH_WPA2_ENTERPRISE:
//						authMode = "WPA2_ENTERPRISE";
//						break;
//					default:
//						authMode = "Unknown\t\t";
//						break;
//					}
//					ESP_LOGI(TAG,"SSID: %-20.20s, MAC: "MACSTR", rssi: %ddbm, authmod: %s, country: %c%c%c",
//							buff,
//							MAC2STR(scanList[i].bssid),
//							scanList[i].rssi,
//							authMode,
//							scanList[i].country.cc[0],
//							scanList[i].country.cc[1],
//							scanList[i].country.cc[2]);
//				}
//			}
//		}else{
//			ESP_LOGE(TAG,"Memory allocate failed!!");
//		}
//		ESP_LOGI(TAG,"Try to free memory!");
//		heap_caps_free(scanList); //�ͷ��ڴ�
//		if(strlen((const char*)wifi_cfg.sta.ssid) != 0){
//			ESP_LOGI(TAG,"Target ssid is: %s",wifi_cfg.sta.ssid);
//			ESP_ERROR_CHECK(esp_wifi_connect());
//		}else ESP_LOGW(TAG,"Password is invalid");
		break;
	default:
		break;
	}
	return ESP_OK;
}

void wifi_init_sta(void)
{
	tcpip_adapter_init(); //create an LwIP core task and initialize LwIP-related work.
    wifi_EventHandle = xEventGroupCreate();
    CountHandle		 =xSemaphoreCreateCounting(MAX_SOC_COUNT,INIT_SOC_COUNT);
	if(wifi_EventHandle == NULL){
		ESP_LOGW(TAG,"Create event group failed!");
	}else ESP_LOGI(TAG,"Create event group success!");
	/*�˴����½����� ��ָ�����ĸ�CPU����*/
	portBASE_TYPE res1 = xTaskCreate(test_task, "test_task",
													ESP_TEST_MAIN_STACK, NULL,
													ESP_TEST_MAIN_PRIO, NULL);
		assert(res1 == pdTRUE);
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler,NULL));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//	esp_wifi_restore();

	wifi_config_t wifi_cfg;
//	wifi_config_t wifi_config = {
//			.sta = {
//				.ssid 			= "2026",			//singfundn
//				.password		= "xianfeng2733",	//dn123456789
//				.scan_method	= WIFI_ALL_CHANNEL_SCAN
//			},
//
//	};

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA,&wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG,"wifi_init_sta is finished");
//	ESP_LOGW(TAG,"Try to scan start");
//	system_restore();
//	ESP_ERROR_CHECK(esp_wifi_scan_start(&scanCfg,true)); //����true��������˼��ʹ��������

	ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA,&wifi_cfg));

    if(strlen((const char *)wifi_cfg.sta.ssid) == 0){
    	ESP_LOGW(TAG,"Default SSID is NULL");
		ESP_LOGI(TAG,"Smartconfig version: %s",esp_smartconfig_get_version());
		ESP_ERROR_CHECK(esp_smartconfig_start(Smart_cfg_cb,1));
    }else{
    	ESP_LOGI(TAG,"The default target ssid is: %s password is: %s",wifi_cfg.sta.ssid,wifi_cfg.sta.password);
//    	bool en;
//    	ESP_ERROR_CHECK(esp_wifi_get_auto_connect(&en));
//    	if(en == true){
//    		ESP_LOGI(TAG,"Enable auto connect!");
//    	}else
//    		ESP_LOGI(TAG,"Disable auto connect!");
    }
    //ESP_ERROR_CHECK(esp_wifi_restore());

}

//�˺���������� ��ô������ᱻɾ�� �������ļ�component/esp32/cpu_start.c  main_task  �������ĸú���
void app_main()
{


    printf("Hello world! second work is success!!!\n");
    //����һ�����ָ��ringbuffer
//    buf_Handle = xRingbufferCreate(1028,RINGBUF_TYPE_NOSPLIT); //����һ��ringbuf
//    buf_Handle = xRingbufferCreate(512,RINGBUF_TYPE_ALLOWSPLIT);

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");


    esp_err_t res = nvs_flash_init();
    if(res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND){
    	ESP_ERROR_CHECK(nvs_flash_erase());
    	res = nvs_flash_init();
    }
    ESP_ERROR_CHECK(res);
    ESP_LOGI(TAG,"This test for STA mode");


    //     res1 = xTaskCreatePinnedToCore(&Send_Ringbuffer, "Send_Ringbuffer",
    //                                                        ESP_SEND_MAIN_STACK, NULL,
    //                                                        ESP_SEND_MAIN_PRIO, NULL, 0);
    //            assert(res1 == pdTRUE);

    wifi_init_sta();
    fflush(stdout); //�������� �����е����ݴ�ӡ����Ȼ��ر�����
//    while(1);
//    esp_restart();
}

int addr_family;
int ip_protocol;
char addr_str[128];
//char rx_buffer[128];
//int sock = 0;
//struct sockaddr_in destAddr;

static void test_task(void* args)
{

	UBaseType_t taskCount 	= 0;

	EventBits_t uxBits;
	char taskName[20];
	struct hostent *hostP = NULL;

	ESP_LOGI(TAG,"test_task is running");
	while(1){
		taskCount++;
		ESP_LOGW(TAG,"test_task is running  %d",taskCount);

		if(wifi_EventHandle != NULL){
			uxBits = xEventGroupWaitBits(wifi_EventHandle,CONNECTED_BIT,false,true,portMAX_DELAY);
			 if((uxBits & CONNECTED_BIT) != 0){
				ESP_LOGI(TAG,"Test task Event CONNECTED_BIT is received!");

//				hostP = lwip_gethostbyname("api.nbcgzn.com");
//				if(hostP != NULL){
//					ESP_LOGI(TAG,"Find %d ip",hostP->h_length);
//					ESP_LOGI(TAG,"Host ip is: %s",ip4addr_ntoa((const ip4_addr_t *)hostP->h_addr));
//				}else {
//					ESP_LOGI(TAG,"Can't get host by name!");
//				}

//				destAddr.sin_addr.s_addr = inet_addr("172.27.35.21");//hostP->h_addr;
//				destAddr.sin_family 	 = AF_INET;
//				destAddr.sin_port 		 = htons(5555);
				addr_family 			 = AF_INET;
				ip_protocol 			 = IPPROTO_IP;
				// ����IP��Ϊ0 Ӧ�õײ���Զ����ñ���IP �˿ڹ̶���7681
				struct sockaddr_in localAddr;
				localAddr.sin_addr.s_addr 	= htonl(INADDR_ANY);
				localAddr.sin_family		= AF_INET;
				localAddr.sin_port			=htons(7681);

//				inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);
				/****************************************************
				 *	sock = socket(addr_family, SOCK_DGRAM, ip_protocol); �е�SOCK_DGRAM ����UDP��ʱ��Ӧ�ô���  TCP��ʱ��Ӧ�ô��� SOCK_STREAM
				 *
				 */
				//�½�һ�� socket
				int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
				if (listen_sock < 0) {
					ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
					break;
				}
				//��socket�󶨵�7681�˿� ��ַ��Ϊ0��ʾ���豸�ϵ����е�IP��7681�˿�
				int err = bind(listen_sock, (struct sockaddr *)&localAddr, sizeof(localAddr));
				if (err < 0) {
					ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
				}
				ESP_LOGI(TAG, "Socket created");
				/**********************************
				 * err = connect(sock,(struct sockaddr *)&destAddr,sizeof(destAddr));
				 */
				//�������� ����7681�˿�
				err = listen(listen_sock,0);
				if(err != 0){
					ESP_LOGI(TAG,"Socket unable to connect: errno %d", errno);
				}
				ESP_LOGI(TAG,"Socket is listening");
				//Ϊaccpet���Ӵ��������ʼ��
				struct sockaddr_in6 sourceAddr;
				uint addrLen = sizeof(sourceAddr);

				while (1) {
					//��ȡ�ź���������������portMAX_DELAY
					if(CountHandle != NULL){
						xSemaphoreTake(CountHandle,portMAX_DELAY);
						UBaseType_t semapCount = uxSemaphoreGetCount(CountHandle);
						ESP_LOGI(TAG,"Semaphore take success semapCount is:%d",semapCount);
					}else ESP_LOGW(TAG,"SemaphoreHandle is NULL");

					//accept�ǻ����������  ���SemaphorTake Ҳһֱ���Ͳ�֪���в��С�
					int sock = accept(listen_sock, (struct sockaddr *)&sourceAddr, &addrLen);
					if (sock < 0) {
						ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
						break;
					}
					ESP_LOGI(TAG, "Socket accepted sock is %d",sock);
					//��ȡ��accept��IP sock �˿���Ϣ����
					struct sockinfo remoteInfo;
					remoteInfo.sock = sock;
					if(sourceAddr.sin6_family == PF_INET){
						remoteInfo.remoteIp = inet_ntoa_r(((struct sockaddr_in *)&sourceAddr)->sin_addr.s_addr,addr_str,sizeof(addr_str) - 1);
						remoteInfo.sa_familyType = PF_INET;

					}else if(sourceAddr.sin6_family == PF_INET6){
						remoteInfo.remoteIp = inet6_ntoa_r(sourceAddr.sin6_addr,addr_str,sizeof(addr_str) - 1);
						remoteInfo.sa_familyType = PF_INET6;
					}
					remoteInfo.remotePort = ntohs(sourceAddr.sin6_port);
					//�ȴ���ȡ������Դ������Ŀɽ�������
					uxBits = xEventGroupWaitBits(wifi_EventHandle,TASK1_BIT|TASK2_BIT,false,false,portMAX_DELAY);
					ESP_LOGW(TAG,"Test task wait TASK_BIT ok!!");
					for(int i = 0; i < MAX_SOC_COUNT; i++){
						if((uxBits & (1 << (i + 1))) != 0){ //����i + 1����Ϊ �¼���־������һλ����CONNECT_BIT��ռ�� TASK2_BIT�Ǵӵ�BIT1��ʼ
							sprintf(taskName,"Tcp_Client%d",i+1);
							//��ӡremoteInfo������Ȼ���ٽ�������
							ESP_LOGI(TAG,"Currently socket NO:%d IP is:%s PORT is:%d",sock,remoteInfo.remoteIp,remoteInfo.remotePort);
//							ESP_LOGE(TAG,"Currently remoteIp len is:%d",strlen(remoteInfo.remoteIp));
							portBASE_TYPE res1 = xTaskCreate(taskList[i], taskName,
																2048, (void *)&remoteInfo,
																7, NULL);
							assert(res1 == pdTRUE);
							break; //����ɹ��Ĵ�����һ�������Ӧ�ý������β�����
						}
					}
					vTaskDelay(200 / portTICK_PERIOD_MS);
				}
				if (listen_sock != -1) {
					ESP_LOGE(TAG, "Shutting down listen_socket and restarting...");
					shutdown(listen_sock, 0);
					close(listen_sock);

				}
			}
		}else{
			ESP_LOGW(TAG,"wifi_EventHandle is NULL!");
		}
		vTaskDelay(5000 / portTICK_PERIOD_MS); //5���Ӻ�������ִ��
		//taskYIELD();
	}
	ESP_LOGE(TAG,"test_task something ERROR inside");
	vTaskDelete(NULL);
}

static void Tcp_Client1(void *args)
{

	char rx_buffer[128];
	struct sockinfo remoteInfo;
	// ��remoteInfo.remoteIp ����һ���Ŀռ� ��ע���ͷ�
	remoteInfo.remoteIp = (char *)heap_caps_malloc(32,MALLOC_CAP_8BIT);
	memset(remoteInfo.remoteIp,0,32);

	remoteInfo.sock			= ((struct sockinfo *)args)->sock;
	remoteInfo.remotePort 	= ((struct sockinfo *)args)->remotePort;
//	ESP_LOGE(TAG,"remoteIP len is %d",strlen(((struct sockinfo *)args)->remoteIp));

	memcpy(remoteInfo.remoteIp,((struct sockinfo *)args)->remoteIp,strlen(((struct sockinfo *)args)->remoteIp));
	//��ӡ����
	ESP_LOGI(TAG,"Tcp_Client1 args is %d",remoteInfo.sock);
	ESP_LOGI(TAG,"Tcp_Client1 was created and task name is:%s",pcTaskGetTaskName(NULL));
	ESP_LOGI(TAG,"Tcp_Client1 socket NO:%d IP is:%s PORT is:%d",remoteInfo.sock,remoteInfo.remoteIp,remoteInfo.remotePort);

	EventBits_t res = xEventGroupClearBits(wifi_EventHandle,TASK1_BIT);
	if((res & TASK1_BIT) != 0) ESP_LOGI(TAG,"TASK1_BIT cleared successfully");
	else{ //������ɹ���ִ��10��
//		int i;
//		for(i = 0;i < 10; i++){
//			res = xEventGroupClearBits(wifi_EventHandle,TASK2_BIT);
//			if(res == pdPASS){
//				ESP_LOGI(TAG,"TASK2_BIT cleared successfully");
//				break;
//			}
//			vTaskDelay(1000 / portTICK_PERIOD_MS);
//		}
//		if(i == 10)
			ESP_LOGE(TAG,"TASK1_BIT clear failed"); //�����ִ��10�ζ����ɹ���ô����־ʧ��
	}

	int keepAlive = 1; // ����keepalive����

	int keepIdle = 20; // ���������60����û���κ���������,�����̽��

	int keepInterval = 5; // ̽��ʱ������ʱ����Ϊ5 ��

	int keepCount = 3; // ̽�Ⳣ�ԵĴ���.�����1��̽������յ���Ӧ��,���2�εĲ��ٷ�.

	setsockopt(remoteInfo.sock,SOL_SOCKET,SO_KEEPALIVE,	(void *)&keepAlive,		sizeof(keepAlive));
	setsockopt(remoteInfo.sock,IPPROTO_TCP,TCP_KEEPIDLE,	(void *)&keepIdle,		sizeof(keepIdle));
	setsockopt(remoteInfo.sock,IPPROTO_TCP,TCP_KEEPINTVL,(void *)&keepInterval, 	sizeof(keepInterval));
	setsockopt(remoteInfo.sock,IPPROTO_TCP,TCP_KEEPCNT,	(void *)&keepCount, 	sizeof(keepCount));

	previousSock = 0;
	while(1)
	{
		int len = recv(remoteInfo.sock,rx_buffer,sizeof(rx_buffer) - 1,0);
		//���Ӵ���
		if(len < 0){
			if(errno == ECONNABORTED){
				ESP_LOGE(TAG,"Tcp_Client1 connection lost");
			}else
				ESP_LOGE(TAG,"Tcp_Client1 Recv failed errno :%d",errno);
			break;
		}
		//���ӶϿ�
		else if(len == 0){
			ESP_LOGW(TAG,"Tcp_Client1 Connection closed Tcp_Client1");
			break;
		}
		//�յ�����
		else{

			rx_buffer[len] = 0; //����ָ��գ��������ǽ��յ�ʲô���Ƕ�������Ϊһ������
			if(previousSock != remoteInfo.sock){
				ESP_LOGW(TAG,"Tcp_Client1 Received %d bytes form %s:%d",len,remoteInfo.remoteIp,remoteInfo.remotePort); //��ӡ�����ǻ�õ����鳤�Ⱥ���Դ��ַ�˿ڵ�
				previousSock = remoteInfo.sock;
			}
			ESP_LOGI(TAG,"%s",rx_buffer);

			int err = send(remoteInfo.sock, rx_buffer, len, 0);
			if (err < 0) {
				ESP_LOGE(TAG, "Tcp_Client1 Error occured during sending: errno %d", errno);
				break;
			}
		}
//		vTaskDelay(100 / portTICK_PERIOD_MS);
		taskYIELD();
	}
	if (remoteInfo.sock != -1) {
		ESP_LOGW(TAG, "Tcp_Client1 Shutting down socket");
		shutdown(remoteInfo.sock, 0);
		close(remoteInfo.sock);
	}

	if(CountHandle != NULL){
		if(xSemaphoreGive(CountHandle) != pdTRUE){
			ESP_LOGE(TAG,"Tcp_Client1 Try to Give semaphore and failed!");
		}else ESP_LOGI(TAG,"Give semaphore success!");
	}
	if(wifi_EventHandle != NULL){

		EventBits_t uxBits = xEventGroupSetBits(wifi_EventHandle,TASK1_BIT);
		if((uxBits & TASK1_BIT) != 0)  	ESP_LOGI(TAG,"Tcp_Client1 set event bit ok");
		else							ESP_LOGI(TAG,"Tcp_Client1 set event bit failed");
	}else ESP_LOGE(TAG,"Tcp_Client1 wifi_EventHandle is NULL");
	ESP_LOGE(TAG,"Tcp_Client1: Something error occurred or connect close,Ready to clear EventGroup and delete Task");
	previousSock = 0;
	vTaskDelete(NULL);
}

static void Tcp_Client2(void *args)
{

	char rx_buffer[128];

	struct sockinfo remoteInfo;
	// ��remoteInfo.remoteIp ����һ���Ŀռ� ��ע���ͷ�
	remoteInfo.remoteIp = (char *)heap_caps_malloc(32,MALLOC_CAP_8BIT);
	memset(remoteInfo.remoteIp,0,32);

	remoteInfo.sock			= ((struct sockinfo *)args)->sock;
	remoteInfo.remotePort 	= ((struct sockinfo *)args)->remotePort;
//	ESP_LOGE(TAG,"remoteIP len is %d",strlen(((struct sockinfo *)args)->remoteIp));
	memcpy(remoteInfo.remoteIp,((struct sockinfo *)args)->remoteIp,strlen(((struct sockinfo *)args)->remoteIp));
	ESP_LOGI(TAG,"Tcp_Client2 args is %d",remoteInfo.sock);
	ESP_LOGI(TAG,"Tcp_Client2 was created and task name is:%s",pcTaskGetTaskName(NULL));
	ESP_LOGI(TAG,"Tcp_Client2 socket NO:%d IP is:%s PORT is:%d",remoteInfo.sock,remoteInfo.remoteIp,remoteInfo.remotePort);

	if(wifi_EventHandle != NULL){
			EventBits_t res = xEventGroupClearBits(wifi_EventHandle,TASK2_BIT);
			if((res & TASK2_BIT) != 0) ESP_LOGI(TAG,"TASK2_BIT cleared successfully");
			else{//������ɹ���ִ��10��
//				int i;
//				for(i = 0;i < 10; i++){
//					res = xEventGroupClearBits(wifi_EventHandle,TASK2_BIT);
//					if((res & TASK2_BIT) != 0){
//						ESP_LOGI(TAG,"TASK2_BIT cleared successfully");
//						break;
//					}
//					vTaskDelay(1000 / portTICK_PERIOD_MS);
//				}
//				if(i == 10)
					ESP_LOGE(TAG,"TASK1_BIT clear failed");//�����ִ��10�ζ����ɹ���ô����־ʧ��
			}
		}else ESP_LOGE(TAG,"Tcp_Client2 wifi_EventHandle is NULL");
	int keepAlive = 1; // ����keepalive����

	int keepIdle = 20; // ���������60����û���κ���������,�����̽��

	int keepInterval = 5; // ̽��ʱ������ʱ����Ϊ5 ��

	int keepCount = 3; // ̽�Ⳣ�ԵĴ���.�����1��̽������յ���Ӧ��,���2�εĲ��ٷ�.

	setsockopt(remoteInfo.sock,SOL_SOCKET,SO_KEEPALIVE,	(void *)&keepAlive,		sizeof(keepAlive));
	setsockopt(remoteInfo.sock,IPPROTO_TCP,TCP_KEEPIDLE,	(void *)&keepIdle,		sizeof(keepIdle));
	setsockopt(remoteInfo.sock,IPPROTO_TCP,TCP_KEEPINTVL,(void *)&keepInterval, 	sizeof(keepInterval));
	setsockopt(remoteInfo.sock,IPPROTO_TCP,TCP_KEEPCNT,	(void *)&keepCount, 	sizeof(keepCount));

	previousSock = 0;
	while(1)
	{
		int len = recv(remoteInfo.sock,rx_buffer,sizeof(rx_buffer) - 1,0);
		//���Ӵ���
		if(len < 0){
			if(errno == ECONNABORTED){
				ESP_LOGE(TAG,"Tcp_Client2 connection lost");
			}else
				ESP_LOGE(TAG,"Tcp_Client2 Recv failed errno :%d",errno);
			break;
		}
		//���ӶϿ�
		else if(len == 0){
			ESP_LOGW(TAG,"Tcp_Client2 Connection closed Tcp_Client2");
			break;
		}
		//�յ�����
		else{
			rx_buffer[len] = 0; //����ָ��գ��������ǽ��յ�ʲô���Ƕ�������Ϊһ������
			if(previousSock != remoteInfo.sock){
				ESP_LOGW(TAG,"Tcp_Client2 Received %d bytes form %s:%d",len,remoteInfo.remoteIp,remoteInfo.remotePort); //��ӡ�����ǻ�õ����鳤�Ⱥ���Դ��ַ�˿ڵ�
				previousSock = remoteInfo.sock;
			}
			ESP_LOGI(TAG,"%s",rx_buffer);

			int err = send(remoteInfo.sock, rx_buffer, len, 0);
			if (err < 0) {
				ESP_LOGE(TAG, "Tcp_Client2 Error occured during sending: errno %d", errno);
				break;
			}
		}
//		vTaskDelay(100 / portTICK_PERIOD_MS);
		taskYIELD();
	}
	if (remoteInfo.sock != -1) {
		ESP_LOGW(TAG, "Tcp_Client2 Shutting down socket");
		shutdown(remoteInfo.sock, 0);
		close(remoteInfo.sock);
	}

	if(CountHandle != NULL){
		if(xSemaphoreGive(CountHandle) != pdTRUE){
			ESP_LOGE(TAG,"Tcp_Client2 Try to Give semaphore and failed!");
		}else ESP_LOGI(TAG,"Tcp_Client2 Give semaphore success!");
	}

	EventBits_t uxBits = xEventGroupSetBits(wifi_EventHandle,TASK2_BIT);
	if((uxBits & TASK2_BIT) != 0)  	ESP_LOGI(TAG,"Tcp_Client2 set event bit ok");
	else							ESP_LOGI(TAG,"Tcp_Client1 set event bit failed");
	ESP_LOGE(TAG,"Tcp_Client2: Something error occurred or connect close,Ready to clear EventGroup and delete Task");
	previousSock = 0;
	vTaskDelete(NULL);
}

