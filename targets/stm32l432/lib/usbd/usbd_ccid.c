#include <stdint.h>
#include "usbd_ccid.h"
#include "usbd_ctlreq.h"
#include "usbd_conf.h"
#include "usbd_core.h"

#include "log.h"

static uint8_t  USBD_CCID_Init (USBD_HandleTypeDef *pdev,
                               uint8_t cfgidx);

static uint8_t  USBD_CCID_DeInit (USBD_HandleTypeDef *pdev,
                                 uint8_t cfgidx);

static uint8_t  USBD_CCID_Setup (USBD_HandleTypeDef *pdev,
                                USBD_SetupReqTypedef *req);

static uint8_t  USBD_CCID_DataIn (USBD_HandleTypeDef *pdev,
                                 uint8_t epnum);

static uint8_t  USBD_CCID_DataOut (USBD_HandleTypeDef *pdev,
                                 uint8_t epnum);

static uint8_t  USBD_CCID_EP0_RxReady (USBD_HandleTypeDef *pdev);


USBD_ClassTypeDef  USBD_CCID =
{
  USBD_CCID_Init,
  USBD_CCID_DeInit,
  USBD_CCID_Setup,
  NULL,                 /* EP0_TxSent, */
  USBD_CCID_EP0_RxReady,
  USBD_CCID_DataIn,
  usb_ccid_recieve_callback,
  NULL,
  NULL,
  NULL,

  NULL,
  NULL,
  NULL,
  NULL,
};

static uint8_t ccidmsg_buf[CCID_DATA_PACKET_SIZE];

static uint8_t  USBD_CCID_Init (USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    uint8_t ret = 0U;
    USBD_CCID_HandleTypeDef   *hcdc;

    //Y
    USBD_LL_OpenEP(pdev, CCID_IN_EP, USBD_EP_TYPE_BULK,
                   CCID_DATA_PACKET_SIZE);

    pdev->ep_in[CCID_IN_EP & 0xFU].is_used = 1U;

    USBD_LL_OpenEP(pdev, CCID_OUT_EP, USBD_EP_TYPE_BULK,
                   CCID_DATA_PACKET_SIZE);

    pdev->ep_out[CCID_OUT_EP & 0xFU].is_used = 1U;


    USBD_LL_OpenEP(pdev, CCID_CMD_EP, USBD_EP_TYPE_INTR, CCID_DATA_PACKET_SIZE);
    pdev->ep_in[CCID_CMD_EP & 0xFU].is_used = 1U;

    static USBD_CCID_HandleTypeDef mem;
    pdev->pClassData = &mem;

    hcdc = (USBD_CCID_HandleTypeDef*) pdev->pClassData;

    // init transfer states
    hcdc->TxState = 0U;
    hcdc->RxState = 0U;

    USBD_LL_PrepareReceive(&Solo_USBD_Device, CCID_OUT_EP, ccidmsg_buf,
                         CCID_DATA_PACKET_SIZE);

    return ret;
}

static uint8_t  USBD_CCID_DeInit (USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  uint8_t ret = 0U;
  //N

  USBD_LL_CloseEP(pdev, CCID_IN_EP);
  pdev->ep_in[CCID_IN_EP & 0xFU].is_used = 0U;

  USBD_LL_CloseEP(pdev, CCID_OUT_EP);
  pdev->ep_out[CCID_OUT_EP & 0xFU].is_used = 0U;

  USBD_LL_CloseEP(pdev, CCID_CMD_EP);
  pdev->ep_in[CCID_CMD_EP & 0xFU].is_used = 0U;

  /* DeInit  physical Interface components */
  if(pdev->pClassData != NULL)
  {
    pdev->pClassData = NULL;
  }

  return ret;
}

/**
  * @brief  USBD_CDC_Setup
  *         Handle the CDC specific requests
  * @param  pdev: instance
  * @param  req: usb requests
  * @retval status
  */
static uint8_t  USBD_CCID_Setup (USBD_HandleTypeDef *pdev,
                                USBD_SetupReqTypedef *req)
{
  USBD_CCID_HandleTypeDef   *hcdc = (USBD_CCID_HandleTypeDef*) pdev->pClassData;
  uint8_t ifalt = 0U;
  uint16_t status_info = 0U;
  uint8_t ret = USBD_OK;
  //N

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
  case USB_REQ_TYPE_CLASS :
    if (req->wLength)
    {
      if (req->bmRequest & 0x80U)
      {
          USBD_CtlSendData (pdev, (uint8_t *)(void *)hcdc->data, req->wLength);
      }
      else
      {
        hcdc->CmdOpCode = req->bRequest;
        hcdc->CmdLength = (uint8_t)req->wLength;

        USBD_CtlPrepareRx (pdev, (uint8_t *)(void *)hcdc->data, req->wLength);
      }
    }
    else
    {

    }
    break;

  case USB_REQ_TYPE_STANDARD:
    switch (req->bRequest)
    {
    case USB_REQ_GET_STATUS:
      if (pdev->dev_state == USBD_STATE_CONFIGURED)
      {
        USBD_CtlSendData (pdev, (uint8_t *)(void *)&status_info, 2U);
      }
      else
      {
        USBD_CtlError (pdev, req);
			  ret = USBD_FAIL;
      }
      break;

    case USB_REQ_GET_INTERFACE:
      if (pdev->dev_state == USBD_STATE_CONFIGURED)
      {
        USBD_CtlSendData (pdev, &ifalt, 1U);
      }
      else
      {
        USBD_CtlError (pdev, req);
			  ret = USBD_FAIL;
      }
      break;

    case USB_REQ_SET_INTERFACE:
      if (pdev->dev_state != USBD_STATE_CONFIGURED)
      {
        USBD_CtlError (pdev, req);
			  ret = USBD_FAIL;
      }
      break;
    case USB_REQ_GET_DESCRIPTOR:
    break;
    default:
      USBD_CtlError (pdev, req);
      ret = USBD_FAIL;
      break;
    }
    break;

  default:
    USBD_CtlError (pdev, req);
    ret = USBD_FAIL;
    break;
  }

  return ret;
}


/**
  * @brief  USBD_CDC_DataIn
  *         Data sent on non-control IN endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t  USBD_CCID_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    //N
  USBD_CCID_HandleTypeDef *hcdc = (USBD_CCID_HandleTypeDef*)pdev->pClassData;
  PCD_HandleTypeDef *hpcd = pdev->pData;

  if(pdev->pClassData != NULL)
  {
    if((pdev->ep_in[epnum].total_length > 0U) && ((pdev->ep_in[epnum].total_length % hpcd->IN_ep[epnum].maxpacket) == 0U))
    {
      /* Update the packet total length */
      pdev->ep_in[epnum].total_length = 0U;

      /* Send ZLP */
      USBD_LL_Transmit (pdev, epnum, NULL, 0U);
    }
    else
    {
      hcdc->TxState = 0U;
    }
    return USBD_OK;
  }
  else
  {
    return USBD_FAIL;
  }
}

uint8_t  USBD_CCID_TransmitPacket(uint8_t * msg, int len)
{
    /* Update the packet total length */
    Solo_USBD_Device.ep_in[CCID_IN_EP & 0xFU].total_length = len;

    /* Transmit next packet */
    USBD_LL_Transmit(&Solo_USBD_Device, CCID_IN_EP, msg,
                   len);

    return USBD_OK;
}

#define CCID_HEADER_SIZE            10
typedef struct
{
    uint8_t type;
    uint32_t len;
    uint8_t slot;
    uint8_t seq;
    uint8_t rsvd;
    uint16_t param;
}  __attribute__((packed)) CCID_HEADER;

void ccid_send_status(CCID_HEADER * c)
{
    uint8_t msg[CCID_HEADER_SIZE];
    memset(msg,0,sizeof(msg));

    msg[0] = CCID_SLOT_STATUS_RES;
    msg[6] = 1;

    USBD_CCID_TransmitPacket(msg, sizeof(msg));

}




void handle_ccid(uint8_t * msg, int len)
{
    CCID_HEADER * h = (CCID_HEADER *) msg;
    switch(h->type)
    {
        case CCID_SLOT_STATUS:
            ccid_send_status(h);
        break;
        default:
            // while(1)
            // {
            //     led_rgb(0xff3520);
            // }
            //Y
            ccid_send_status(h);
        break;
    }
}

/**
  * @brief  USBD_CDC_DataOut
  *         Data received on non-control Out endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
uint8_t usb_ccid_recieve_callback(USBD_HandleTypeDef *pdev, uint8_t epnum)
{

    USBD_CCID_HandleTypeDef *hcdc = (USBD_CCID_HandleTypeDef*) pdev->pClassData;

    /* Get the received data length */
    hcdc->RxLength = USBD_LL_GetRxDataSize (pdev, epnum);

    /* USB data will be immediately processed, this allow next USB traffic being
    NAKed till the end of the application Xfer */
    if(pdev->pClassData != NULL)
    {
        // printf1(TAG_GREEN,"ccid>> ");
        // dump_hex1(TAG_GREEN, hcdc->RxBuffer, hcdc->RxLength);

        handle_ccid(hcdc->RxBuffer, hcdc->RxLength);

        return USBD_OK;
    }
    else
    {


        while(1)
            {
                led_rgb(0xff3520);
            }

        return USBD_FAIL;
    }
}


/**
  * @brief  USBD_CDC_EP0_RxReady
  *         Handle EP0 Rx Ready event
  * @param  pdev: device instance
  * @retval status
  */
static uint8_t  USBD_CCID_EP0_RxReady (USBD_HandleTypeDef *pdev)
{
    USBD_CCID_HandleTypeDef   *hcdc = (USBD_CCID_HandleTypeDef*) pdev->pClassData;

    // if((pdev->pUserData != NULL) && (hcdc->CmdOpCode != 0xFFU))
    // {
    //     hcdc->CmdOpCode = 0xFFU;
    // }
    return USBD_OK;
}