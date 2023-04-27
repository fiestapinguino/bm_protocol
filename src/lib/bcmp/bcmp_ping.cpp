#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

#include "bcmp.h"
#include "bcmp_neighbors.h"
#include "bcmp_ping.h"
#include "device_info.h"
#include "uptime.h"

static uint64_t _ping_request_time;
static uint32_t _bcmp_seq;
static uint8_t* _expected_payload;

err_t bcmp_send_ping_request(uint64_t node_id, const ip_addr_t *addr, uint8_t* payload, uint16_t payload_len) {

  uint16_t echo_len = sizeof(bcmp_echo_request_t) + payload_len;

  uint8_t *echo_req_buff = static_cast<uint8_t *>(pvPortMalloc(echo_len));
  configASSERT(echo_req_buff);

  memset(echo_req_buff, 0, echo_len);

  bcmp_echo_request_t *echo_req = static_cast<bcmp_echo_request_t *>(echo_req_buff);

  echo_req->target_node_id = node_id;
  echo_req->id = (uint16_t)getNodeId(); // TODO - make this a randomly generated number
  echo_req->seq_num = _bcmp_seq++;
  echo_req->payload_len = payload_len;

  if (payload != NULL) {
    memcpy(&echo_req->payload[0], payload, payload_len);
  }

  printf("PING (%" PRIx64 "): %" PRIu16 " data bytes\n", echo_req->target_node_id, echo_req->payload_len);

  err_t rval = bcmp_tx(addr, BCMP_ECHO_REQUEST, static_cast<uint8_t*>(echo_req), sizeof(*echo_req));

  _ping_request_time = uptimeGetMicroSeconds();

  vPortFree(echo_req_buff);

  return rval;
}

err_t bcmp_send_ping_reply(bcmp_echo_reply_t *echo_reply, const ip_addr_t *addr) {

  return bcmp_tx(addr, BCMP_ECHO_REPLY, static_cast<uint8_t*>(echo_reply), sizeof(*echo_reply));
}

err_t bcmp_process_ping_request(bcmp_echo_request_t *echo_req, const ip_addr_t *src, const ip_addr_t *dst) {
  (void) src;
  // TODO - we will need to reply one the same port, but pass on the request onto the other port?
  // or will it auto pass on based on the multicast and we only have to reply if we have the correct nodeID?
  configASSERT(echo_req);
  if ((echo_req->target_node_id == 0) || (getNodeId() == echo_req->target_node_id)) {
    echo_req->target_node_id = getNodeId();
    return bcmp_send_ping_reply(static_cast<bcmp_echo_reply_t*>(echo_req), dst);
  }

  return ERR_OK;
}

err_t bcmp_process_ping_reply(bcmp_echo_reply_t *echo_reply){
  configASSERT(echo_reply);
  // TODO - once we have random numbers working we can then use a static
  //        number to check
  if((uint16_t)getNodeId() == echo_reply->id){
    uint64_t diff = uptimeGetMicroSeconds() - _ping_request_time;
    printf("🏓 %" PRIu16 " bytes from %" PRIx64 " bcmp_seq=%" PRIu32 " time=%" PRIu64 " ms\n", echo_reply->payload_len, echo_reply->node_id, echo_reply->seq_num, diff/1000);
  }

  return ERR_OK;
}
