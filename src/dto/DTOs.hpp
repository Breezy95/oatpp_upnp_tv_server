#ifndef MyDTOs_hpp
#define MyDTOs_hpp

#include "oatpp/core/Types.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)


class ActionArgDTO : public oatpp::DTO {
  DTO_INIT(ActionArgDTO, DTO)

  DTO_FIELD(String, name);
  DTO_FIELD(String, direction)="in";
  DTO_FIELD(String, relatedStateVariable)="";
  DTO_FIELD(String, value)="";
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
  DTO_FIELD(String, xmlString);
};

class BaseTVDTO : public oatpp::DTO {
  DTO_INIT(BaseTVDTO, DTO)
  DTO_FIELD(String, deviceAddress);
  DTO_FIELD(String, devUDN);
  DTO_FIELD(Int32, statusCode);
};

class ActionDTO : public BaseTVDTO{
  DTO_INIT(ActionDTO, BaseTVDTO)
  DTO_FIELD(String, actionUrl, "actionUrl");
  DTO_FIELD(String, serviceId, "serviceId");
  DTO_FIELD(String, actionName);
  DTO_FIELD(String, serviceType, "serviceType");
  DTO_FIELD(List<Object<ActionArgDTO>>, actionArgsList);
};

class SetResourceUriDTO: public ActionDTO{
  DTO_INIT(SetResourceUriDTO, ActionDTO)
  DTO_FIELD(Int32, isAudio) = 0;
  DTO_FIELD(Int32, serverGen) = 0;
  DTO_FIELD(Fields<String>, fileInfo)= {};
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
};

class MessageDto : public oatpp::DTO {
  
  DTO_INIT(MessageDto, DTO)
  DTO_FIELD(Int32, statusCode);
  DTO_FIELD(String, message);
  
};
 
class UpnpErrorDTO : public MessageDto {
  DTO_INIT(UpnpErrorDTO, MessageDto)
  DTO_FIELD(Int16, upnpErrorCode);
};



// Between client and server funcs
class BaseControllerReqDTO : public oatpp::DTO {
DTO_INIT(BaseControllerReqDTO, DTO)
DTO_FIELD(String, actionUrl);
DTO_FIELD(String, serviceType);
DTO_FIELD(String, serviceId);
};




class BaseControllerResponseDTO : public oatpp::DTO {
  DTO_INIT(BaseControllerResponseDTO, DTO)
  DTO_FIELD(Int8, statusCode);

};

class ListDeviceDTO : public UpnpSearchRequest {
DTO_INIT(ListDeviceDTO, UpnpSearchRequest)
DTO_FIELD(String,deviceType);
DTO_FIELD(String, serviceType);
DTO_FIELD(String, version) = "1";
DTO_FIELD(String, locationUrl);
};

#include OATPP_CODEGEN_END(DTO)

#endif /* MyDTOs_hpp */
