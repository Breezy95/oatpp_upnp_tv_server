#ifndef MyDTOs_hpp
#define MyDTOs_hpp

#include "oatpp/core/Types.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)

class ActionArgsDTO : public oatpp::DTO {
  DTO_INIT(ActionArgsDTO, DTO)

  DTO_FIELD(String, name);
  DTO_FIELD(String, direction);
  DTO_FIELD(String, relatedStateVariable);
};
// TV DTOS for simple reqs from client to controller

class DeviceInfoDTO : public oatpp::DTO {
  DTO_INIT(DeviceInfoDTO, DTO)
  
  DTO_FIELD(String, deviceId);
  DTO_FIELD(String, deviceType);
  DTO_FIELD(String, friendlyName);
  DTO_FIELD(String, manufacturer);
  DTO_FIELD(String, modelName);
  DTO_FIELD(String, serviceType);
  DTO_FIELD(String, serviceId);
  DTO_FIELD(String, ipAddr);
  DTO_FIELD(Int32, port);
  DTO_FIELD(String, locationUrl);
};

class BaseTVDTO : public oatpp::DTO {
  DTO_INIT(BaseTVDTO, BaseTVDTO)
  DTO_FIELD(String, deviceAddress);
  DTO_FIELD(String, actionURL); 
  DTO_FIELD(String, serviceType); 
  DTO_FIELD(String, devUDN);
  DTO_FIELD(Int32, statusCode);
};
class UpnpDeviceListResponse : public oatpp::DTO {
  DTO_INIT(UpnpDeviceListResponse, DTO)
  DTO_FIELD(List<Object<DeviceInfoDTO>>, devices) = oatpp::List<Object<DeviceInfoDTO>>::createShared();

};

class UpnpSearchRequest : public BaseTVDTO {
  DTO_INIT(UpnpSearchRequest, BaseTVDTO)
  DTO_FIELD(String, searchType);
  DTO_FIELD(Int8, mx) = 3;
};

class EnqueueVidRequestDTO : public BaseTVDTO { 
  DTO_INIT(EnqueueVidRequestDTO, BaseTVDTO) 
  DTO_FIELD(String, userAgent, "user-id");
  DTO_FIELD(String, mediaResourceUrl, "resourceUrl");
  DTO_FIELD(String,  xmlAction, "xmlActionDocument");
  //const void *Cookie in the future we are going to want to implement some form of value keeping

};

class MessageDto : public oatpp::DTO {
  
  DTO_INIT(MessageDto, oatpp::DTO)
  DTO_FIELD(Int32, statusCode);
  DTO_FIELD(String, message);
  
};


class UpnpErrorDTO : public MessageDto {
  DTO_INIT(UpnpErrorDTO, MessageDto)
  DTO_FIELD(Int16, upnpErrorCode);
};



// Between client and server NOT controller
class BaseControllerReqDTO : public oatpp::DTO {
DTO_INIT(BaseControllerReqDTO, DTO)
DTO_FIELD(String, actionUrl);
DTO_FIELD(String, serviceType);
DTO_FIELD(String, serviceId);
};


class ActionDTO : public BaseControllerReqDTO {
  DTO_INIT(ActionDTO, BaseControllerReqDTO)
  DTO_FIELD(String, actionName);
  DTO_FIELD(Fields<Object<ActionArgsDTO>>, actionArgsList);
};

#include OATPP_CODEGEN_END(DTO)

#endif /* MyDTOs_hpp */
