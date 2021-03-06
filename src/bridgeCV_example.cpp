#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <cmath>

#define Pi 3.14159265359

namespace enc = sensor_msgs::image_encodings;

static const char WINDOW[] = "Image window";

class ImageConverter
{
  ros::NodeHandle nh_;
  image_transport::ImageTransport it_;
  image_transport::Subscriber image_sub_;
  image_transport::Publisher image_pub_;
  int thresh;
  double F, CameraCalibMat[3][3];

public:
  ImageConverter()
    : it_(nh_)
  {

    image_pub_ = it_.advertise("out", 1);
    image_sub_ = it_.subscribe("/Environment/CameraMain/image", 1, &ImageConverter::imageCb, this);

    cv::namedWindow(WINDOW);
    int temp=150;
    cv::createTrackbar( " Threshold:", WINDOW, &temp, 255, NULL);
    temp=51962;
    cv::createTrackbar( "   Focus:  ", WINDOW, &temp, 100000, NULL);
    CameraCalibMat[0][2]=320;
    CameraCalibMat[1][2]=240;
  }

  ~ImageConverter()
  {
    cv::destroyWindow(WINDOW);
  }

  void imageCb(const sensor_msgs::ImageConstPtr& msg)
  {
    cv_bridge::CvImagePtr cv_ptr;
    try
    {
      cv_ptr = cv_bridge::toCvCopy(msg, enc::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
      ROS_ERROR("cv_bridge exception: %s", e.what());
      return;
    }

   // Convert image to gray and blur it
    cv::Mat src_gray(cv_ptr->image.rows,cv_ptr->image.cols,CV_8UC1);
	//cv::cvtColor( cv_ptr->image, src_gray, CV_BGR2GRAY );
	int from_to[] = { 2,0 };
	cv::mixChannels(& cv_ptr->image,1,&src_gray,1,from_to,1);
	cv::blur( src_gray, src_gray, cv::Size(3,3) );
    /// Detect edges using Threshold
    cv::Mat threshold_output;
    thresh=cv::getTrackbarPos(" Threshold:", WINDOW);
    cv::threshold( src_gray, threshold_output, thresh, 255, cv::THRESH_BINARY );
    F=cv::getTrackbarPos("   Focus:  ", WINDOW)*1.0/100.0;
    /// Find contours
    cv::vector<cv::vector<cv::Point> > contours;
    cv::vector<cv::Vec4i> hierarchy;

    cv::findContours( threshold_output, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cv::Point(0, 0) );
    //cv::vector<cv::RotatedRect> rotRect[contours.size()];
    unsigned int max_count=0;
    unsigned int max_i =0;
    for( unsigned int i = 0; i < contours.size(); i++ )
    {
    	//cv::Mat pointsf;
    	//pointsfcv::Mat(contours[i]).convertTo(pointsf,cv::CV_
    	size_t count = contours[i].size();
		if( count < 20 )
			continue;
		if (count >= 20 && count > max_count)
		{
			max_count=count;
			max_i=i;
		}
    	//cv::RotatedRect box=cv::fitEllipse(cv::Mat(contours[i]));
    	//cv::ellipse(cv_ptr->image, box, cv::Scalar(0,255,255), 1, CV_AA);
    }
    if (contours[max_i].size() >= 20)
    {
    	cv::RotatedRect box=cv::fitEllipse(cv::Mat(contours[max_i]));
    	cv::ellipse(cv_ptr->image, box, cv::Scalar(0,255,255), 1, CV_AA);
    	cv::Point2f rect_points[4]; box.points( rect_points );
	    for( int j = 0; j < 4; j++ )
		cv::line( cv_ptr->image, rect_points[j], rect_points[(j+1)%4], cv::Scalar(255,0,255), 1, 8 );
	    //char str_buffer [100];
	    cv::string str_buffer;
	    str_buffer=cv::format("Height = %5.2f -|- Width = %5.2f",box.size.height/2,box.size.width/2);
	    cv::putText(cv_ptr->image,str_buffer,cv::Point(10,10),cv::FONT_HERSHEY_PLAIN,0.8,cv::Scalar(255,255,255),1,8);
	    str_buffer=cv::format("  X    = %5.2f -|-    Y  = %5.2f",box.center.x-320,-box.center.y+240);
	    cv::putText(cv_ptr->image,str_buffer,cv::Point(10,20),cv::FONT_HERSHEY_PLAIN,0.8,cv::Scalar(255,255,255),1,8);
	    str_buffer=cv::format("Angle  = %5.2f",box.angle);
	    cv::putText(cv_ptr->image,str_buffer,cv::Point(10,30),cv::FONT_HERSHEY_PLAIN,0.8,cv::Scalar(255,255,255),1,8);
	    //std::sprintf(char* str,"Row: %f",box);
	    //char* str;
	    //std::sprintf(str,"abc %d",max_i);
		double W=(box.size.width+1e-10)/2; double fi=(box.angle+1e-10)*3.14/180; double H=(box.size.height+1e-10)/2; double X=box.center.x+1e-10-320; double Y=-box.center.y+1e-10+240;
		//fi=3.14/2;X=0;Y=0;
		double a = std::pow(std::cos(fi)/W,2)+std::pow(std::sin(fi)/H,2);
		//a=1000*std::cos(fi)/W;
		double b = (1/std::pow(H,2)-1/std::pow(W,2))*std::cos(fi)*sin(fi);
		double c = std::pow(std::cos(fi)/H,2)+std::pow(std::sin(fi)/W,2);
		double d = X*std::pow(cos(fi),2)/std::pow(W,2)+(1/std::pow(H,2)-1/std::pow(W,2))*cos(fi)*sin(fi)*Y+X*std::pow(sin(fi),2)/std::pow(H,2);
		double e = Y*std::pow(cos(fi),2)/std::pow(H,2)+(1/std::pow(H,2)-1/std::pow(W,2))*cos(fi)*sin(fi)*X+Y*std::pow(sin(fi),2)/std::pow(W,2);
		double f = ((std::pow(H,2)+std::pow(W,2))*(std::pow(X,2)+std::pow(Y,2))+(H-W)*(H+W)*((X-Y)*(X+Y)*cos(2*fi)-2*X*Y*sin(2*fi)))/(2*std::pow(F*H*W,2))-1;
		d=d/F;e=e/F;
		double m[3][3] = {{a, b, d}, {b, c, e}, {d, e, f}};
		CvMat Mat = cvMat(3, 3, CV_64F, m);
		CvMat* evec = cvCreateMat(3,3,CV_64FC1);
		CvMat* eval = cvCreateMat(1,3,CV_64FC1);
		cvZero(evec);
		cvZero(eval);
		cvBox2Conic(box, &Mat);
		cvEigenVV(&Mat, evec, eval, DBL_EPSILON, 0, 0);
		CvScalar L1 = cvGet2D( eval, 0, 0 );
		CvScalar L2 = cvGet2D( eval, 0, 1 );
		CvScalar L3 = cvGet2D( eval, 0, 2 );
		//cvBox2Conic(box, evec);
		cv::putText(cv_ptr->image,"  Ellipse matrix",cv::Point(10,40),cv::FONT_HERSHEY_PLAIN,0.8,cv::Scalar(255,255,255),1,8);
		str_buffer=cv::format("| %+5.2e %+5.2e %+5.2e |    | %+5.2e %+5.2e %+5.2e |",Mat.data.db[0],Mat.data.db[1],Mat.data.db[2],evec->data.db[0],evec->data.db[1],evec->data.db[2]);
		cv::putText(cv_ptr->image,str_buffer,cv::Point(10,50),cv::FONT_HERSHEY_PLAIN,0.8,cv::Scalar(255,255,255),1,8);
		str_buffer=cv::format("| %+5.2e %+5.2e %+5.2e |    | %+5.2e %+5.2e %+5.2e |",(Mat.data.db+Mat.width)[0],(Mat.data.db+Mat.width)[1],(Mat.data.db+Mat.width)[2],(evec->data.db+evec->width)[0],(evec->data.db+evec->width)[1],(evec->data.db+evec->width)[2]);
		cv::putText(cv_ptr->image,str_buffer,cv::Point(10,60),cv::FONT_HERSHEY_PLAIN,0.8,cv::Scalar(255,255,255),1,8);
		str_buffer=cv::format("| %+5.2e %+5.2e %+5.2e |    | %+5.2e %+5.2e %+5.2e |",(Mat.data.db+2*Mat.width)[0],(Mat.data.db+2*Mat.width)[1],(Mat.data.db+2*Mat.width)[2],(evec->data.db+2*evec->width)[0],(evec->data.db+2*evec->width)[1],(evec->data.db+2*evec->width)[2]);
		cv::putText(cv_ptr->image,str_buffer,cv::Point(10,70),cv::FONT_HERSHEY_PLAIN,0.8,cv::Scalar(255,255,255),1,8);
		str_buffer=cv::format("Lambda =[ %+5.2e  %+5.2e  %+5.2e ]",L1.val[0],L2.val[0],L3.val[0]);
		cv::putText(cv_ptr->image,str_buffer,cv::Point(10,90),cv::FONT_HERSHEY_PLAIN,0.8,cv::Scalar(255,255,255),1,8);
		//Print Z distance
		double z=1.262126246*L2.val[0]/std::sqrt(-L3.val[0]*L1.val[0]);
		str_buffer=cv::format("Z =[ %+5.2e  ]",z);
		cv::putText(cv_ptr->image,str_buffer,cv::Point(10,110),cv::FONT_HERSHEY_PLAIN,0.8,cv::Scalar(255,255,255),1,8);
		CvMat *C = cvCreateMat( 3, 1, CV_64FC1);
		//cvTranspose(eval,C);
		//cvMatMul( evec, C, C);
		double* ptr = C->data.db;
		ptr[0]=std::sqrt((L1.val[0]-L2.val[0])/(L1.val[0]-L3.val[0]));
		ptr=ptr+C->width;
		ptr[0]=0;
		ptr=ptr+C->width;
		ptr[0]=-std::sqrt((L2.val[0]-L3.val[0])/(L1.val[0]-L3.val[0]));
		cvMatMul( evec, C, C);
		str_buffer=cv::format("Ori =[ %+5.2e  %+5.2e  %+5.2e ]",(C->data.db)[0],(C->data.db+C->width)[0],(C->data.db+2*C->width)[0]);
		cv::putText(cv_ptr->image,str_buffer,cv::Point(10,130),cv::FONT_HERSHEY_PLAIN,0.8,cv::Scalar(255,255,255),1,8);
		ptr = C->data.db;
		ptr[0]=z*L3.val[0]*std::sqrt((L1.val[0]-L2.val[0])/(L1.val[0]-L3.val[0]));///L2.val[0];
		ptr=ptr+C->width;
		ptr[0]=0;
		ptr=ptr+C->width;
		ptr[0]=-z*L1.val[0]*std::sqrt((L2.val[0]-L3.val[0])/(L1.val[0]-L3.val[0]));///L2.val[0];
		cvMatMul( evec, C, C);
		str_buffer=cv::format("Pos =[ %+5.2e  %+5.2e  %+5.2e ]",(C->data.db)[0],(C->data.db+C->width)[0],(C->data.db+2*C->width)[0]);
		cv::putText(cv_ptr->image,str_buffer,cv::Point(10,150),cv::FONT_HERSHEY_PLAIN,0.8,cv::Scalar(255,255,255),1,8);
		cvReleaseMat(&evec);
		cvReleaseMat(&eval);
		line( cv_ptr->image,cvPoint(0,cv_ptr->image.rows/2),cvPoint(cv_ptr->image.cols,cv_ptr->image.rows/2),cvScalar(1,1,0),2,8);
		line( cv_ptr->image,cvPoint(cv_ptr->image.cols/2,0),cvPoint(cv_ptr->image.cols/2,cv_ptr->image.rows),cvScalar(1,1,0),2,8);
    }
    cv::waitKey(3);
    cv::imshow(WINDOW, cv_ptr->image);
    //cv::imshow(WINDOW,threshold_output);
    image_pub_.publish(cv_ptr->toImageMsg());
  }
  void cvBox2Conic(CvBox2D ellipseBox,CvMat* conicQ)
  {
	  //Rotation & Translation
	  double R[3][3];
	  R[0][0] = std::cos(ellipseBox.angle*Pi/180.0);
	  R[0][1] = -sin(ellipseBox.angle*Pi/180.0);
	  R[0][2] = ellipseBox.center.x-CameraCalibMat[0][2];
	  R[1][0] = sin(ellipseBox.angle*Pi/180.0);
	  R[1][1] = cos(ellipseBox.angle*Pi/180.0);
	  R[1][2] = ellipseBox.center.y-CameraCalibMat[1][2];
	  R[2][0] = 0; R[2][1] = 0; R[2][2] = 1;
	  CvMat MatR = cvMat(3, 3, CV_64F, R);
	  CvMat *TransInvMatR = cvCreateMat(3, 3, CV_64FC1);
	  //cvInvert(&MatR, &MatR, CV_LU);
	  cvTranspose(&MatR,TransInvMatR);
	  //Centered Ellipse Conic Equation
	  double Q[3][3];
	  Q[0][0] = 1;
	  Q[1][1] = (ellipseBox.size.width*ellipseBox.size.width)/(ellipseBox.size.height*ellipseBox.size.height);
	  Q[2][2] = -(ellipseBox.size.width*ellipseBox.size.width)/(4.0);
	  Q[0][1] = 0; Q[0][2] = 0; Q[1][0] = 0; Q[1][2] = 0; Q[2][0] = 0; Q[2][1] = 0;
	  CvMat MatQ = cvMat(3, 3, CV_64F, Q);
	  // Matrix multiplication (R^-1)^T * Q * R^-1
	  cvMatMul(TransInvMatR,&MatQ,&MatQ);
	  cvMatMul(&MatQ,&MatR,&MatQ);
	  // Focal length and packing
	  *(MatQ.data.db+2)=*(MatQ.data.db+2)/F;
	  *(MatQ.data.db+MatQ.width+2)=*(MatQ.data.db+MatQ.width+2)/F;
	  *(MatQ.data.db+2*MatQ.width+0)=*(MatQ.data.db+2*MatQ.width)/(F);
	  *(MatQ.data.db+2*MatQ.width+1)=*(MatQ.data.db+2*MatQ.width+1)/(F);
	  *(MatQ.data.db+2*MatQ.width+2)=*(MatQ.data.db+2*MatQ.width+2)/(F*F);
	  for(int i=0;i<3;i++)
	  {
		  for(int j=0;j<3;j++)
		  {
			  if (MatQ.data.db[0] > 0)
			  {
				  *(conicQ->data.db+i*conicQ->width+j)=*(MatQ.data.db+i*MatQ.width+j);
			  }
			  else
			  {
				  *(conicQ->data.db+i*conicQ->width+j)=-*(MatQ.data.db+i*MatQ.width+j);
			  }
	  	  }
	  }
  }
};

int main(int argc, char** argv)
{
  ros::init(argc, argv, "image_converter");
  ImageConverter ic;
  ros::spin();
  return 0;
}
