#include <depthai/depthai.hpp>

#include "depthai/capabilities/ImgFrameCapability.hpp"
#include "depthai/common/CameraBoardSocket.hpp"
#include "depthai/pipeline/node/host/Record.hpp"
#include "utility.hpp"

#ifndef DEPTHAI_HAVE_OPENCV_SUPPORT
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
    dai::Pipeline pipeline(true);
    auto cam = pipeline.create<dai::node::Camera>()->build(dai::CameraBoardSocket::CAM_A);
    auto display = pipeline.create<dai::node::Display>();
    auto videoEncoder = pipeline.create<dai::node::VideoEncoder>();
    auto record = pipeline.create<dai::node::RecordVideo>();

    std::string path = argc > 1 ? argv[1] : getDefaultRecordingPath();
    record->setRecordVideoFile(path + std::string(".mp4"));
    record->setRecordMetadataFile(path + std::string(".mcap"));

    auto* camOut = cam->requestOutput({1280, 960}, dai::ImgFrame::Type::NV12, dai::ImgResizeMode::CROP, 30.f);

    videoEncoder->setProfile(dai::VideoEncoderProperties::Profile::H264_MAIN);

    camOut->link(videoEncoder->input);
    camOut->link(display->input);
    videoEncoder->out.link(record->input);

    pipeline.run();  // Let the display node stop the pipeline
}
