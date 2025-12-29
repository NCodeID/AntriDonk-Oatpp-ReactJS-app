#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/Types.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/HttpRouter.hpp"
#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/websocket/ConnectionHandler.hpp"
#include "oatpp/websocket/Handshaker.hpp"
#include "oatpp/websocket/WebSocket.hpp"

#include <mutex>
#include <deque>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace std;

#include OATPP_CODEGEN_BEGIN(DTO)

class PersonDto : public oatpp::DTO
{
  DTO_INIT(PersonDto, DTO)
  DTO_FIELD(Int32, nomorAntrian);
  DTO_FIELD(String, nama);
  DTO_FIELD(String, NIK);
  DTO_FIELD(String, Alamat);
};

class ApiResponse : public oatpp::DTO
{
  DTO_INIT(ApiResponse, DTO)
  DTO_FIELD(Boolean, success);
  DTO_FIELD(String, message);
  DTO_FIELD(Object<PersonDto>, data);
};

class QueueListDto : public oatpp::DTO
{
  DTO_INIT(QueueListDto, DTO)
  DTO_FIELD(Vector<Object<PersonDto>>, items);
  DTO_FIELD(Int32, total);
  DTO_FIELD(Int32, currentCounter);
};

#include OATPP_CODEGEN_END(DTO)

class AdvancedQueue
{
private:
  std::deque<oatpp::Object<PersonDto>> dq;
  std::mutex mtx;
  int lastCounter = 0;

public:
  void enqueue(const oatpp::Object<PersonDto> &person)
  {
    std::lock_guard<std::mutex> lock(mtx);
    lastCounter++;
    person->nomorAntrian = lastCounter;
    dq.push_back(person);
  }

  oatpp::Object<PersonDto> dequeue()
  {
    std::lock_guard<std::mutex> lock(mtx);
    if (dq.empty())
      return nullptr;
    auto p = dq.front();
    dq.pop_front();
    return p;
  }

  oatpp::Object<PersonDto> skip()
  {
    std::lock_guard<std::mutex> lock(mtx);
    if (dq.empty())
      return nullptr;
    auto p = dq.front();
    dq.pop_front();
    dq.push_back(p);
    return p;
  }

  std::vector<oatpp::Object<PersonDto>> list()
  {
    std::lock_guard<std::mutex> lock(mtx);
    return std::vector<oatpp::Object<PersonDto>>(dq.begin(), dq.end());
  }

  int getCurrentCounter()
  {
    std::lock_guard<std::mutex> lock(mtx);
    return lastCounter;
  }

  void seedData(const std::string &jsonString,
                const std::shared_ptr<oatpp::parser::json::mapping::ObjectMapper> &mapper)
  {
    try
    {
      auto items = mapper->readFromString<oatpp::Vector<oatpp::Object<PersonDto>>>(jsonString.c_str());
      std::lock_guard<std::mutex> lock(mtx);
      for (auto &item : *items)
      {
        if (!item->nomorAntrian || item->nomorAntrian == 0)
        {
          lastCounter++;
          item->nomorAntrian = lastCounter;
        }
        else if (item->nomorAntrian > lastCounter)
        {
          lastCounter = item->nomorAntrian;
        }
        dq.push_back(item);
      }
      OATPP_LOGI("Queue", "Berhasil seeding %d data. Counter terakhir: %d", items->size(), lastCounter);
    }
    catch (const std::exception &e)
    {
      OATPP_LOGE("Queue", "Gagal seeding data: %s", e.what());
    }
  }
};

static void broadcast(const std::string &msg,
                      std::vector<oatpp::websocket::WebSocket *> &clients,
                      std::mutex &mtx)
{
  std::lock_guard<std::mutex> lock(mtx);
  for (auto *ws : clients)
  {
    ws->sendOneFrameText(msg.c_str());
  }
}

#include OATPP_CODEGEN_BEGIN(ApiController)

class QueueController : public oatpp::web::server::api::ApiController
{
private:
  std::shared_ptr<AdvancedQueue> queue;
  std::vector<oatpp::websocket::WebSocket *> *clients;
  std::mutex *mtx;
  std::shared_ptr<oatpp::parser::json::mapping::ObjectMapper> mapper;

  void broadcastQueueState(const std::string &eventType,
                           oatpp::Object<PersonDto> current = nullptr)
  {
    auto dto = QueueListDto::createShared();
    dto->items = oatpp::Vector<Object<PersonDto>>::createShared();
    for (auto &i : queue->list())
    {
      dto->items->push_back(i);
    }
    dto->total = (v_int32)dto->items->size();
    dto->currentCounter = queue->getCurrentCounter();

    std::stringstream ss;
    ss << "{";
    ss << "\"type\":\"" << eventType << "\",";
    ss << "\"queue\":" << mapper->writeToString(dto)->c_str();
    if (current)
    {
      ss << ",\"person\":" << mapper->writeToString(current)->c_str();
    }
    ss << "}";

    broadcast(ss.str(), *clients, *mtx);
  }

public:
  QueueController(const std::shared_ptr<oatpp::parser::json::mapping::ObjectMapper> &m,
                  const std::shared_ptr<AdvancedQueue> &q,
                  std::vector<oatpp::websocket::WebSocket *> *c,
                  std::mutex *mt)
      : oatpp::web::server::api::ApiController(m), queue(q), clients(c), mtx(mt), mapper(m) {}

  static std::shared_ptr<QueueController> createShared(
      const std::shared_ptr<oatpp::parser::json::mapping::ObjectMapper> &m,
      const std::shared_ptr<AdvancedQueue> &q,
      std::vector<oatpp::websocket::WebSocket *> *c,
      std::mutex *mt)
  {
    return std::make_shared<QueueController>(m, q, c, mt);
  }
  std::shared_ptr<OutgoingResponse> createDtoResponseWithCors(Status status, const oatpp::Void &dto)
  {
    auto response = createDtoResponse(status, dto);
    response->putHeader("Access-Control-Allow-Origin", "*");
    response->putHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->putHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept");
    response->putHeader("Access-Control-Max-Age", "3600");
    return response;
  }

  std::shared_ptr<OutgoingResponse> createCorsResponse(Status status)
  {
    auto response = oatpp::web::protocol::http::outgoing::ResponseFactory::createResponse(status);
    response->putHeader("Access-Control-Allow-Origin", "*");
    response->putHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->putHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept");
    response->putHeader("Access-Control-Max-Age", "3600");
    return response;
  }

  ENDPOINT("POST", "/enqueue", enqueue, BODY_DTO(Object<PersonDto>, body))
  {
    if (!body || !body->nama || !body->NIK)
    {
      return createResponse(Status::CODE_400, "Nama dan NIK wajib diisi");
    }
    queue->enqueue(body);
    broadcastQueueState("queue:update");
    auto resp = ApiResponse::createShared();
    resp->success = true;
    resp->message = "Berhasil masuk antrian";
    resp->data = body;
    return createDtoResponseWithCors(Status::CODE_200, resp);
  }

  ENDPOINT("POST", "/dequeue", dequeue)
  {
    auto p = queue->dequeue();
    auto resp = ApiResponse::createShared();
    if (p)
    {
      resp->success = true;
      resp->message = "Pemanggilan berhasil";
      resp->data = p;
      broadcastQueueState("queue:current", p);
    }
    else
    {
      resp->success = false;
      resp->message = "Antrian kosong";
      broadcastQueueState("queue:update");
    }
    return createDtoResponseWithCors(Status::CODE_200, resp);
  }

  ENDPOINT("POST", "/skip", skip)
  {
    auto p = queue->skip();
    auto resp = ApiResponse::createShared();
    if (p)
    {
      resp->success = true;
      resp->message = "Penerima Bantuan di-skip (pindah ke belakang)";
      resp->data = p;
      broadcastQueueState("queue:current", p);
    }
    else
    {
      resp->success = false;
      resp->message = "Antrian kosong, tidak bisa skip";
      broadcastQueueState("queue:update");
    }
    return createDtoResponseWithCors(Status::CODE_200, resp);
  }

  ENDPOINT("GET", "/queue", list)
  {
    auto items = queue->list();
    auto dto = QueueListDto::createShared();
    dto->items = oatpp::Vector<Object<PersonDto>>::createShared();
    for (auto &i : items)
    {
      dto->items->push_back(i);
    }
    dto->total = (v_int32)items.size();
    dto->currentCounter = queue->getCurrentCounter();
    broadcastQueueState("queue:update");
    return createDtoResponseWithCors(Status::CODE_200, dto);
  }

  ENDPOINT("OPTIONS", "/enqueue", options_enqueue)
  {
    return createCorsResponse(Status::CODE_204);
  }

  ENDPOINT("OPTIONS", "/dequeue", options_dequeue)
  {
    return createCorsResponse(Status::CODE_204);
  }

  ENDPOINT("OPTIONS", "/skip", options_skip)
  {
    return createCorsResponse(Status::CODE_204);
  }

  ENDPOINT("OPTIONS", "/queue", options_queue)
  {
    return createCorsResponse(Status::CODE_204);
  }
};

class WsSocketInstanceListener
    : public oatpp::websocket::ConnectionHandler::SocketInstanceListener
{
private:
  std::vector<oatpp::websocket::WebSocket *> *clients;
  std::mutex *mtx;

public:
  WsSocketInstanceListener(std::vector<oatpp::websocket::WebSocket *> *c,
                           std::mutex *m) : clients(c), mtx(m) {}

  void onAfterCreate(const oatpp::websocket::WebSocket &socket,
                     const std::shared_ptr<const ParameterMap> &) override
  {
    auto *ws = const_cast<oatpp::websocket::WebSocket *>(&socket);
    std::lock_guard<std::mutex> lock(*mtx);
    clients->push_back(ws);
    ws->sendOneFrameText(R"({"type":"connected","status":"ok"})");
  }

  void onBeforeDestroy(const oatpp::websocket::WebSocket &socket) override
  {
    auto *ws = const_cast<oatpp::websocket::WebSocket *>(&socket);
    std::lock_guard<std::mutex> lock(*mtx);
    clients->erase(std::remove(clients->begin(), clients->end(), ws), clients->end());
  }
};

class WsController : public oatpp::web::server::api::ApiController
{
private:
  std::vector<oatpp::websocket::WebSocket *> clients;
  std::mutex mtx;
  std::shared_ptr<oatpp::websocket::ConnectionHandler> wsHandler;

public:
  WsController(const std::shared_ptr<ObjectMapper> &mapper)
      : ApiController(mapper)
  {
    wsHandler = oatpp::websocket::ConnectionHandler::createShared();
    wsHandler->setSocketInstanceListener(
        std::make_shared<WsSocketInstanceListener>(&clients, &mtx));
  }

  ENDPOINT("GET", "/ws", ws,
           REQUEST(std::shared_ptr<IncomingRequest>, request))
  {
    return oatpp::websocket::Handshaker::serversideHandshake(
        request->getHeaders(), wsHandler);
  }

  std::vector<oatpp::websocket::WebSocket *> *getClients() { return &clients; }
  std::mutex *getMutex() { return &mtx; }
};

#include OATPP_CODEGEN_END(ApiController)

static std::string readFile(const std::string &filename)
{
  std::ifstream t(filename);
  if (!t.is_open())
    return "";
  std::stringstream buffer;
  buffer << t.rdbuf();
  return buffer.str();
}

int main()
{
  oatpp::base::Environment::init();

  auto router = oatpp::web::server::HttpRouter::createShared();
  auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();

  auto queue = std::make_shared<AdvancedQueue>();

  std::string initialJson = readFile("initial_data.json");
  if (!initialJson.empty())
  {
    queue->seedData(initialJson, objectMapper);
  }
  else
  {
    OATPP_LOGW("Main", "File initial_data.json tidak ditemukan, counter mulai dari 0.");
  }

  auto wsController = std::make_shared<WsController>(objectMapper);

  auto queueController = QueueController::createShared(objectMapper, queue,
                                                       wsController->getClients(),
                                                       wsController->getMutex());

  router->addController(queueController);
  router->addController(wsController);

  auto connectionHandler = oatpp::web::server::HttpConnectionHandler::createShared(router);
  auto connectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared(
      {"0.0.0.0", 8000, oatpp::network::Address::IP_4});
  oatpp::network::Server server(connectionProvider, connectionHandler);

  OATPP_LOGI("Server", "Berjalan di port 8000. AntriDonkBackend.");
  server.run();

  oatpp::base::Environment::destroy();
  return 0;
}
