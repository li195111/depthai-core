#include "depthai/pipeline/node/host/XLinkInHost.hpp"

#include "depthai/pipeline/datatype/StreamMessageParser.hpp"
#include "depthai/xlink/XLinkConnection.hpp"
#include "depthai/xlink/XLinkConstants.hpp"
#include "depthai/xlink/XLinkStream.hpp"
#include "spdlog/fmt/bin_to_hex.h"
#include "spdlog/fmt/chrono.h"

// libraries
#include "depthai/pipeline/datatype/MessageGroup.hpp"
#include "depthai/pipeline/datatype/Tracklets.hpp"
#include "utility/Logging.hpp"

namespace dai {
namespace node {
// XLinkInHost::XLinkInHost(std::shared_ptr<XLinkConnection> conn, std::string streamName) : conn(std::move(conn)), streamName(std::move(streamName)){};

void XLinkInHost::setStreamName(const std::string& name) {
    streamName = name;
}

void XLinkInHost::setConnection(std::shared_ptr<XLinkConnection> conn) {
    this->conn = std::move(conn);
    std::lock_guard<std::mutex> lock(mtx);
    isWaitingForReconnect.notify_all();
}

void XLinkInHost::disconnect() {
    isDisconnected = true;
    std::lock_guard<std::mutex> lock(mtx);
    isWaitingForReconnect.notify_all();
}


// Reads int from little endian format
inline int readIntLE(uint8_t* data) {
    return data[0] + data[1] * 256 + data[2] * 256 * 256 + data[3] * 256 * 256 * 256;
}

void XLinkInHost::run() {
    // Create a stream for the connection
    bool reconnect = true;
    while(reconnect) {
        reconnect = false;
        XLinkStream stream(std::move(conn), streamName, 1);
        while(isRunning()) {
            try {
                // Blocking -- parse packet and gather timing information
                auto packet = stream.readMove();

                const std::uint32_t debug_packet_length = packet.length - 16;
                const DatatypeEnum object_type = static_cast<DatatypeEnum>(readIntLE(packet.data + debug_packet_length - 8));

                if(object_type == DatatypeEnum::Tracklets) {
                    logger::info("SECTION 1");
                    logger::info("Received Tracklets message");
                    std::vector<uint8_t> tracklet_data(packet.data, packet.data + 88);
                    logger::info("Tracklet data: {}", spdlog::to_hex(tracklet_data));
                    logger::info("Byte 34: {}", tracklet_data[33]);
                }   

                const auto t1Parse = std::chrono::steady_clock::now();
                const auto msg = StreamMessageParser::parseMessage(std::move(packet));
                if(std::dynamic_pointer_cast<MessageGroup>(msg) != nullptr) {
                    auto msgGrp = std::static_pointer_cast<MessageGroup>(msg);
                    for(auto& msg : msgGrp->group) {
                        auto dpacket = stream.readMove();
                        msg.second = StreamMessageParser::parseMessage(&dpacket);
                    }
                }

                if(std::dynamic_pointer_cast<Tracklets>(msg) != nullptr) {
                    logger::info("SECTION 2");
                    logger::info("Received Tracklets message");
                    auto tracklets = std::static_pointer_cast<Tracklets>(msg);  
                    for(auto& tracklet : tracklets->tracklets) {
                        logger::info("Tracklet ID: {}", tracklet.id);
                        logger::info("Tracklet label: {}", tracklet.label);
                        logger::info("Tracklet age: {}", tracklet.age);
                        logger::info("Tracklet status: {}", static_cast<std::int32_t>(tracklet.status));
                    }
                    logger::info("--------------------------------");
                }

                const auto t2Parse = std::chrono::steady_clock::now();

                // Trace level debugging
                if(logger::get_level() == spdlog::level::trace) {
                    std::vector<std::uint8_t> metadata;
                    DatatypeEnum type;
                    msg->serialize(metadata, type);
                    logger::trace("Received message from device ({}) - parsing time: {}, data size: {}, object type: {} object data: {}",
                                  streamName,
                                  std::chrono::duration_cast<std::chrono::microseconds>(t2Parse - t1Parse),
                                  msg->data->getSize(),
                                  static_cast<std::int32_t>(type),
                                  spdlog::to_hex(metadata));
                }

                out.send(msg);
                // // Add 'data' to queue
                // if(!queue.push(msg)) {
                //     throw std::runtime_error(fmt::format("Underlying queue destructed"));
                // }

                // Call callbacks
                // {
                //     std::unique_lock<std::mutex> l(callbacksMtx);
                //     for(const auto& kv : callbacks) {
                //         const auto& callback = kv.second;
                //         try {
                //             callback(name, msg);
                //         } catch(const std::exception& ex) {
                //             logger::error("Callback with id: {} throwed an exception: {}", kv.first, ex.what());
                //         }
                //     }
                // }
            } catch(const std::exception& ex) {
                if(isRunning()) {
                    auto exceptionMessage = fmt::format("Communication exception - possible device error/misconfiguration. Original message '{}'", ex.what());
                    logger::error(exceptionMessage);
                    std::unique_lock<std::mutex> lck(mtx);
                    logger::info("Waiting for reconnect (XLINKINHOST)\n");
                    isWaitingForReconnect.wait(lck);
                    if(isDisconnected) throw std::runtime_error(exceptionMessage);
                    logger::info("Reconnected (XLINKINHOST)\n");
                    reconnect = true;
                    break;
                } else {
                    // If the node is not running, we can safely ignore the exception, since it's expected
                    logger::info("XLinkInHost node stopped - exception: {}", ex.what());
                    break;
                }
            }
        }
    }
}

}  // namespace node
}  // namespace dai