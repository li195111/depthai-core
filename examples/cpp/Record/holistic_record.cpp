#include <filesystem>

#include "depthai/common/CameraBoardSocket.hpp"
#include "depthai/depthai.hpp"
#include "depthai/pipeline/node/host/Display.hpp"
#include "depthai/utility/RecordReplay.hpp"
#include "utility.hpp"
#ifndef DEPTHAI_MERGED_TARGET
    #error This example needs OpenCV support, which is not available on your system
#endif

#ifdef _WIN32
    #include <random>
std::string generateRandomString(size_t length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);

    std::string result;
    result.reserve(length);
    for(size_t i = 0; i < length; ++i) {
        result += alphanum[dis(gen)];
    }
    return result;
}

char* win_mkdtemp(char* templateName) {
    // Extract prefix
    std::string prefix;
    std::string suffix = "XXXXXX";
    std::string templateStr(templateName);

    size_t pos = templateStr.find(suffix);
    if(pos != std::string::npos) {
        prefix = templateStr.substr(0, pos);
    } else {
        prefix = templateStr;
    }

    // Generate random name
    std::string randomPart = generateRandomString(6);
    std::string dirName = prefix + randomPart;

    // Copy result to template buffer, ensuring it doesn't overflow
    strncpy(templateName, dirName.c_str(), strlen(templateName));

    return templateName;
}
#endif

std::string getDefaultRecordingPath() {
    auto isTest = std::getenv("RUNNING_AS_TEST");
    if(isTest && std::string(isTest) == "1") {
        // If running as test save to temporary directory
        char tmpTemplate[] = "holistic_recording_XXXXXX";
#ifdef _WIN32
        char* tmpName = win_mkdtemp(tmpTemplate);
#else
        char* tmpName = mkdtemp(tmpTemplate);
#endif
        auto tmpDir = std::filesystem::temp_directory_path() / tmpName;
        std::filesystem::create_directory(tmpDir);
        return tmpDir.string();
    } else {
        return ".";
    }
}

int main(int argc, char** argv) {
    dai::Pipeline pipeline;
    auto camA = pipeline.create<dai::node::Camera>()->build(dai::CameraBoardSocket::CAM_A);
    auto* camAOut = camA->requestOutput({600, 400});
    auto camB = pipeline.create<dai::node::Camera>()->build(dai::CameraBoardSocket::CAM_B);
    auto* camBOut = camB->requestOutput({600, 400});
    auto camC = pipeline.create<dai::node::Camera>()->build(dai::CameraBoardSocket::CAM_C);
    auto* camCOut = camC->requestOutput({600, 400});

    auto imu = pipeline.create<dai::node::IMU>();

    auto display = pipeline.create<dai::node::Display>();

    // enable ACCELEROMETER_RAW at 500 hz rate
    imu->enableIMUSensor(dai::IMUSensor::ACCELEROMETER_RAW, 500);
    // enable GYROSCOPE_RAW at 400 hz rate
    imu->enableIMUSensor(dai::IMUSensor::GYROSCOPE_RAW, 400);
    imu->setBatchReportThreshold(100);

    camAOut->link(display->input);
    auto q = imu->out.createOutputQueue();

    auto camAqueue = camAOut->createOutputQueue();
    auto camBqueue = camBOut->createOutputQueue();
    auto camCqueue = camCOut->createOutputQueue();

    dai::RecordConfig config;
    config.outputDir = argc > 1 ? std::string(argv[1]) : getDefaultRecordingPath();
    config.videoEncoding.enabled = true;  // Use video encoding
    config.videoEncoding.bitrate = 0;     // Automatic
    config.videoEncoding.profile = dai::VideoEncoderProperties::Profile::H264_MAIN;

    pipeline.enableHolisticRecord(config);

    pipeline.start();

    auto start = std::chrono::steady_clock::now();

    try {
        while(std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
            auto imuData = q->get<dai::IMUData>();
            auto imuPackets = imuData->packets;
            for(auto& imuPacket : imuPackets) {
                auto& acceleroValues = imuPacket.acceleroMeter;
                auto& gyroValues = imuPacket.gyroscope;

                // printf("Accelerometer [m/s^2]: x: %.3f y: %.3f z: %.3f \n", acceleroValues.x, acceleroValues.y, acceleroValues.z);
                // printf("Gyroscope [rad/s]: x: %.3f y: %.3f z: %.3f \n", gyroValues.x, gyroValues.y, gyroValues.z);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } catch(...) {
    }

    pipeline.stop();
}
