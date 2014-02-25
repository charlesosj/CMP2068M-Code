
#include <iostream>
#include <Windows.h>
#include <NuiApi.h>
#include "Winuser.h"

using namespace std;
int main()
{
	cout << "start" <<endl ;

	for (;;)
	{
		
	}
	//initialize variables
	int first =1;
	int centerX =0,centerY = 0 ;
	//holds x and y of curso
	POINT cursorLoc ;

	//starts the kinect sensor and states we need to use skeletal tracking
	NuiInitialize(NUI_INITIALIZE_FLAG_USES_SKELETON) ;
	//enable kinect seating support
	NuiSkeletonTrackingEnable( 0, NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT );
	// name of a variable that holds the kinect data
	NUI_SKELETON_FRAME ourframe;//holds the kinect data like a variable

	//inifinite loop
	while (1) //For all of time
	{
		//Get a frame and stuff it into ourframe
		NuiSkeletonGetNextFrame(0, &ourframe); 
		//kinect can get up to six skeltons, so for everyone we do all the code, we could only do this once maybe
		for (int i = 0; i < 6; i++) //Six times
		{
			
			//if a person is tracked
			if (ourframe.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED) 
			{
				first++;
				//calibrate on the 200th loop, set, center x and y
				if ( first ==200)
				{
					cout<< "tracked" << endl;
					//get position of head x and y and set our center x and y once!
					centerX = ourframe.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD].x *1000 ;
					centerY =ourframe.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD].y*1000;

					cout <<"center calibrated at" <<centerX <<"   " <<centerY <<endl ;
					first =201;
				}
				else if (first>150)
				{		
					//get x and y again
					double x = ourframe.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD].x *1000 ;
					double y =ourframe.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD].y *1000;
					cout <<"x ="<<x << "     " << "y" <<y  <<endl;

					//get mouse position
					GetCursorPos(&cursorLoc);
					ShowCursor(0) ;


					// this is were we move the mouse based on the changes of the kinect we need to tweak
					int x1 = centerX -10;
					int x2 = centerX +10;
					int y1 = centerY -10;
					int y2 = centerY +50;



					//x9
					if (x > x2  )
					{
						cursorLoc.x ++;
						SetCursorPos(cursorLoc.x,cursorLoc.y);
					}
					if (x <x1)
					{
						cursorLoc.x --;
						SetCursorPos(cursorLoc.x,cursorLoc.y);
					}


					//y
					if (y >y2)
					{
						cursorLoc.y --;
						SetCursorPos(cursorLoc.x,cursorLoc.y);
					}
					if (y <y1)
					{
						cursorLoc.y ++;
						SetCursorPos(cursorLoc.x,cursorLoc.y);

					}

				}
					
			}
		}



	}
}



