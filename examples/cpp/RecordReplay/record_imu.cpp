
#include <cstdio>
#include <iostream>

#include "depthai/pipeline/node/host/Record.hpp"
#include "utility.hpp"

// Includes common necessary includes for development using depthai library
#include "depthai/depthai.hpp"

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
    // Create pipeline
    dai::Pipeline pipeline;

    // Define sources and outputs
    auto imu = pipeline.create<dai::node::IMU>();
    auto record = pipeline.create<dai::node::RecordMetadataOnly>();

    // enable ACCELEROMETER_RAW at 500 hz rate
    imu->enableIMUSensor(dai::IMUSensor::ACCELEROMETER_RAW, 500);
    // enable GYROSCOPE_RAW at 400 hz rate
    imu->enableIMUSensor(dai::IMUSensor::GYROSCOPE_RAW, 400);

    std::string recordFile = argc > 1 ? argv[1] : getDefaultRecordingPath();
    record->setRecordFile(recordFile + ".mcap");

    imu->out.link(record->input);

    pipeline.start();

    std::this_thread::sleep_for(std::chrono::seconds(10));

    pipeline.stop();
}
