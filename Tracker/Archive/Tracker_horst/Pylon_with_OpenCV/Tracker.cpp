// Pylon_with_OpenCV.cpp

/*
    Note: Before getting started, Basler recommends reading the Programmer's Guide topic
    in the pylon C++ API documentation that gets installed with pylon.
    If you are upgrading to a higher major version of pylon, Basler also
    strongly recommends reading the Migration topic in the pylon C++ API documentation.

    This sample illustrates how to grab and process images using the CInstantCamera class and OpenCV.
    The images are grabbed and processed asynchronously, i.e.,
    while the application is processing a buffer, the acquisition of the next buffer is done
    in parallel.
	
	OpenCV is used to demonstrate an image display, an image saving and a video recording.

    The CInstantCamera class uses a pool of buffers to retrieve image data
    from the camera device. Once a buffer is filled and ready,
    the buffer can be retrieved from the camera object for processing. The buffer
    and additional image data are collected in a grab result. The grab result is
    held by a smart pointer after retrieval. The buffer is automatically reused
    when explicitly released or when the smart pointer object is destroyed.
*/

#include "stdafx.h"
#include <iomanip>
#include <iostream>
#include <fstream>
// Define if images are to be saved.
// '0'- no; '1'- yes.
#define saveImages 0
// Define if video is to be recorded.
// '0'- no; '1'- yes.
#define recordVideo 0

// Include files to use OpenCV API.
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/video.hpp>
#include <opencv2/features2d.hpp>


// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#ifdef PYLON_WIN_BUILD
#    include <pylon/PylonGUI.h>
#endif

#include "threshold.h"

void on_low_h_thresh_trackbar(int, void *);
void on_high_h_thresh_trackbar(int, void *);
void on_low_s_thresh_trackbar(int, void *);
void on_high_s_thresh_trackbar(int, void *);
void on_low_v_thresh_trackbar(int, void *);
void on_high_v_thresh_trackbar(int, void *);

int low_h = 67, low_s = 149, low_v = 42;
int high_h = 146, high_s = 255, high_v = 95;
int image_h = 1000;
int image_w = 1000;

// Namespace for using pylon objects.
using namespace Pylon;

// Namespace for using OpenCV objects.
using namespace cv;
// Namespace for using cout.
using namespace std;

using namespace GenApi;
// Number of images to be grabbed.
static const uint32_t c_countOfImagesToGrab = 10000;

int main(int argc, char* argv[])
{

	// The exit code of the sample application.
    int exitCode = 0;

    // Automagically call PylonInitialize and PylonTerminate to ensure the pylon runtime system
    // is initialized during the lifetime of this object.
    Pylon::PylonAutoInitTerm autoInitTerm;
	//PylonInitialize(); // has a matching "Terminate" statement at the very end.

    try
    {
        // Create an instant camera object with the camera device found first.
        CInstantCamera camera( CTlFactory::GetInstance().CreateFirstDevice());

        // Print the model name of the camera.
		cout << "Using device " << camera.GetDeviceInfo().GetVendorName() << " " << camera.GetDeviceInfo().GetModelName() << endl;

		// Get a camera nodemap in order to access camera parameters.
		INodeMap& nodemap = camera.GetNodeMap();
		// Open the camera before accessing any parameters.
		camera.Open();

		// The parameter MaxNumBuffer can be used to control the count of buffers
        // allocated for grabbing. The default value of this parameter is 10.
        camera.MaxNumBuffer = 10;
		CIntegerPtr(nodemap.GetNode("Width"))->SetValue(image_w);
		CIntegerPtr(nodemap.GetNode("Height"))->SetValue(image_h);
		CIntegerPtr(nodemap.GetNode("OffsetX"))->SetValue(1064);
		CIntegerPtr(nodemap.GetNode("OffsetY"))->SetValue(544);
		CIntegerPtr width = nodemap.GetNode("Width");
		CIntegerPtr height = nodemap.GetNode("Height");
		CEnumerationPtr gainAuto(nodemap.GetNode("GainAuto"));
		CEnumerationPtr exposureAuto(nodemap.GetNode("ExposureAuto"));
		CEnumerationPtr exposureMode(nodemap.GetNode("ExposureMode"));
	
		// set gain and exposure time
		cout << "Setting auto gain to off" << endl;
		//gainAuto->FromString("Off");
		cout << "Setting auto exposure to off" << endl;
		exposureAuto->FromString("Off");
		CFloatPtr gain(nodemap.GetNode("Gain"));
		double newGain = gain->GetMin()+2;
		gain->SetValue(newGain);
		// exposure time
		double d = CFloatPtr(nodemap.GetNode("ExposureTime"))->GetValue();
		CFloatPtr(nodemap.GetNode("ExposureTime"))->SetValue(20000.);

		// Set the pixel format to RGB8 8bit
		CEnumerationPtr(nodemap.GetNode("PixelFormat"))->FromString("RGB8");

		// Declare an integer variable to count the number of grabbed images
		// and create image file names with ascending number.
		int grabbedImages= 0;

		// Create an OpenCV video creator.
		VideoWriter cvVideoCreator;
		// Define the video file name.
		std::string videoFileName= "openCvVideo.avi";
		// Define the video frame size.
		cv::Size frameSize= Size((int)width->GetValue(), (int)height->GetValue());

		// Set the codec type and the frame rate. You have 3 codec options here.
		// The frame rate should match or be lower than the camera acquisition frame rate.
		//cvVideoCreator.open(videoFileName, CV_FOURCC('D','I','V','X'), 30, frameSize, true);
		//cvVideoCreator.open(videoFileName, CV_FOURCC('M','P','4','2'), 30, frameSize, true); 
		cvVideoCreator.open(videoFileName, CV_FOURCC('M','J','P','G'), 30, frameSize, true);

		// create text file
		ofstream output_timestamps;
		output_timestamps.open("export_timestamps.csv");
		output_timestamps << "frame,timestamp,framerate\n";

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // Start the grabbing of c_countOfImagesToGrab images.
        // The camera device is parameterized with a default configuration which
        // sets up free-running continuous acquisition.
		camera.StartGrabbing( c_countOfImagesToGrab, GrabStrategy_LatestImageOnly); // GrabStrategy_LatestImageOnly GrabStrategy_OneByOne

        // This smart pointer will receive the grab result data.
        CGrabResultPtr ptrGrabResult;

        // Camera.StopGrabbing() is called automatically by the RetrieveResult() method
        // when c_countOfImagesToGrab images have been retrieved.
		
		cv::Mat mat8_uc3_c(1000, 1000, CV_8UC3);

		// start tracking code here.

		int64_t last_tic;

		namedWindow("OpenCV Tracker", CV_WINDOW_NORMAL); // other options: CV_AUTOSIZE, CV_FREERATIO, CV_WINDOW_NORMAL
		resizeWindow("OpenCV Tracker", 500, 500);
		// Create another OpenCV display window.
		namedWindow("OpenCV Tracker Thresh", CV_WINDOW_NORMAL); // other options: CV_AUTOSIZE, CV_FREERATIO, CV_WINDOW_NORMAL
		resizeWindow("OpenCV Tracker Thresh", 500,885);
		//-- Trackbars to set thresholds for HSV values
		createTrackbar("Low H", "OpenCV Tracker Thresh", &low_h, 255, on_low_h_thresh_trackbar);
		createTrackbar("High H", "OpenCV Tracker Thresh", &high_h, 255, on_high_h_thresh_trackbar);
		createTrackbar("Low S", "OpenCV Tracker Thresh", &low_s, 255, on_low_s_thresh_trackbar);
		createTrackbar("High S", "OpenCV Tracker Thresh", &high_s, 255, on_high_s_thresh_trackbar);
		createTrackbar("Low V", "OpenCV Tracker Thresh", &low_v, 255, on_low_v_thresh_trackbar);
		createTrackbar("High V", "OpenCV Tracker Thresh", &high_v, 255, on_high_v_thresh_trackbar);


        while ( camera.IsGrabbing())
        {
			CCommandPtr(nodemap.GetNode("TimestampLatch"))->Execute();
			// Get the timestamp value
			int64_t tic = CIntegerPtr(nodemap.GetNode("TimestampLatchValue"))->GetValue();
			if (grabbedImages == 0)
			{
				int64_t last_tic = tic;
			}
			double framerate_calc = 1/((tic - last_tic)/ 1000000000.); // GHz
			cout << grabbedImages << "| Framerate: " << framerate_calc << " fps" << endl;
			output_timestamps << grabbedImages << "," << tic << "," << framerate_calc << "\n";

     		last_tic = tic;

            // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
            camera.RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);

            // Image grabbed successfully?
            if (ptrGrabResult->GrabSucceeded())
            {
				grabbedImages++;
				// Create an OpenCV image from a grabbed image.
				cv::Mat mat8_uc3(image_h, image_w, CV_8UC3, (uintmax_t *)ptrGrabResult->GetBuffer());
				
				// Start tracking here 
				cv::Mat imgScribble = GetThresholdedImage(mat8_uc3, low_h, high_h, low_s, high_s, low_v, high_v);

				// invert channel order
				cv::cvtColor(mat8_uc3, mat8_uc3_c, cv::COLOR_BGR2RGB);
				cv::Mat scaled_ = mat8_uc3_c * 2; // only for viewing purposes (make it all a bit brighter)

				// Set saveImages to '1' to save images.
				if (saveImages) {					
					// Create the current image name for saving.
					std::ostringstream s;
					// Create image name files with ascending grabbed image numbers.
					s<< "export/image_" << grabbedImages << ".jpg";
					std::string imageName(s.str());
					// Save an OpenCV image.

					imwrite(imageName, scaled_);
				}

				// Set recordVideo to '1' to record AVI video file.
				if (recordVideo)
					cvVideoCreator.write(scaled_);

				// Create an OpenCV display window.
				imshow( "OpenCV Tracker", scaled_);
				imshow("OpenCV Tracker Thresh", imgScribble);
				imgScribble.release();
				waitKey(1);

#ifdef PYLON_WIN_BUILD
                // Display the grabbed image in pylon.
                //Pylon::DisplayImage(1, ptrGrabResult);
#endif
            }
            else
            {
                cout << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << endl;
            }
        }

		// close timestamp file 
		output_timestamps.close();


		// Release the video file on leaving.
		if (recordVideo)
			cvVideoCreator.release();
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling.
        cout << "An exception occurred." << endl
        << e.GetDescription() << endl;
        exitCode = 1;
    }

    // Comment the following two lines to disable waiting on exit.
    cerr << endl << "Press Enter to exit." << endl;
    while( cin.get() != '\n');
	// Releases all pylon resources. 
	//PylonTerminate();
    return exitCode;
}


void on_low_h_thresh_trackbar(int, void *)
{
	low_h = min(high_h - 1, low_h);
	setTrackbarPos("Low H", "Object Detection", low_h);
}
void on_high_h_thresh_trackbar(int, void *)
{
	high_h = max(high_h, low_h + 1);
	setTrackbarPos("High H", "Object Detection", high_h);
}


void on_low_s_thresh_trackbar(int, void *)
{
	low_s = min(high_s - 1, low_s);
	setTrackbarPos("Low S", "Object Detection", low_s);
}
void on_high_s_thresh_trackbar(int, void *)
{
	high_s = max(high_s, low_s + 1);
	setTrackbarPos("High S", "Object Detection", high_s);
}

void on_low_v_thresh_trackbar(int, void *)
{
	low_v = min(high_v - 1, low_v);
	setTrackbarPos("Low V", "Object Detection", low_v);
}
void on_high_v_thresh_trackbar(int, void *)
{
	high_v = max(high_v, low_v + 1);
	setTrackbarPos("High V", "Object Detection", high_v);
}

