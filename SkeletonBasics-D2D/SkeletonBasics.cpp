//------------------------------------------------------------------------------
// <copyright file="SkeletonBasics.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------
//remember to sort screen resolution
#include "stdafx.h"
#include "wtypes.h"
#include <Windows.h>
//#include <strsafe.h>
#include "SkeletonBasics.h"
#include "resource.h"
#include "KinectAudioStream.h"
//#include "SpeechBasics.h"
#define INITGUID
#include <guiddef.h>
#include <strsafe.h>
static const float g_JointThickness = 3.0f;
static const float g_TrackedBoneThickness = 6.0f;
static const float g_InferredBoneThickness = 1.0f;
int screenX = 0, screenY = 0;
int centerX = 0, centerY = 0, count = 0;
int leftshoulderY = 0, rightshoulderY = 0;
void Movemouse(int x, int y, int Rsy, int Lsy);
void enterkey(char keyinput);
LPCWSTR CSkeletonBasics::GrammarFileName = L"SpeechBasics-D2D.grxml";
// This is the class ID we expect for the Microsoft Speech recognizer.
// Other values indicate that we're using a version of sapi.h that is
// incompatible with this sample.
DEFINE_GUID(CLSID_ExpectedRecognizer, 0x495648e7, 0xf7ab, 0x4267, 0x8e, 0x0f, 0xca, 0xfb, 0x7a, 0x33, 0xc1, 0x60);
/// <summary>
/// Entry point for the application
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="hPrevInstance">always 0</param>
/// <param name="lpCmdLine">command line arguments</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
/// <returns>status</returns>
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	screenX = GetSystemMetrics(SM_CXSCREEN);
	screenY = GetSystemMetrics(SM_CYSCREEN);
	if (CLSID_ExpectedRecognizer != CLSID_SpInprocRecognizer)
	{
		MessageBoxW(NULL, L"This sample was compiled against an incompatible version of sapi.h.\nPlease ensure that Microsoft Speech SDK and other sample requirements are installed and then rebuild application.", L"Missing requirements", MB_OK | MB_ICONERROR);

		return EXIT_FAILURE;
	}
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (SUCCEEDED(hr))
	{
		{
			CSkeletonBasics application;
			application.Run(hInstance, nCmdShow);
		}

		CoUninitialize();
	}
	return EXIT_SUCCESS;
}

/// <summary>
/// Constructor
/// </summary>
CSkeletonBasics::CSkeletonBasics() :
m_pD2DFactory(NULL),
m_hNextSkeletonEvent(INVALID_HANDLE_VALUE),
m_pSkeletonStreamHandle(INVALID_HANDLE_VALUE),
m_bSeatedMode(false),
m_pRenderTarget(NULL),
m_pBrushJointTracked(NULL),
m_pBrushJointInferred(NULL),
m_pBrushBoneTracked(NULL),
m_pBrushBoneInferred(NULL),
m_pKinectAudioStream(NULL),
m_pSpeechStream(NULL),
m_pSpeechRecognizer(NULL),
m_pSpeechContext(NULL),
m_pSpeechGrammar(NULL),
m_hSpeechEvent(INVALID_HANDLE_VALUE),
m_pNuiSensor(NULL)

{
	ZeroMemory(m_Points, sizeof(m_Points));
}

/// <summary>
/// Destructor
/// </summary>
CSkeletonBasics::~CSkeletonBasics()
{
	if (m_pNuiSensor)
	{
		m_pNuiSensor->NuiShutdown();
	}

	if (m_hNextSkeletonEvent && (m_hNextSkeletonEvent != INVALID_HANDLE_VALUE))
	{
		CloseHandle(m_hNextSkeletonEvent);
	}

	// clean up Direct2D objects
	DiscardDirect2DResources();

	// clean up Direct2D
	SafeRelease(m_pD2DFactory);

	SafeRelease(m_pNuiSensor);
	SafeRelease(m_pKinectAudioStream);
	SafeRelease(m_pSpeechStream);
	SafeRelease(m_pSpeechRecognizer);
	SafeRelease(m_pSpeechContext);
	SafeRelease(m_pSpeechGrammar);
}

/// <summary>
/// Creates the main window and begins processing
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
int CSkeletonBasics::Run(HINSTANCE hInstance, int nCmdShow)
{
	MSG       msg = { 0 };
	WNDCLASS  wc = { 0 };

	// Dialog custom window class
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.cbWndExtra = DLGWINDOWEXTRA;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
	wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP));
	wc.lpfnWndProc = DefDlgProcW;
	wc.lpszClassName = L"SkeletonBasicsAppDlgWndClass";

	if (!RegisterClassW(&wc))
	{
		return 0;
	}

	// Create main application window
	HWND hWndApp = CreateDialogParamW(
		hInstance,
		MAKEINTRESOURCE(IDD_APP),
		NULL,
		(DLGPROC)CSkeletonBasics::MessageRouter,
		reinterpret_cast<LPARAM>(this));

	// Show window
	ShowWindow(hWndApp, nCmdShow);

	const int eventCount = 1;
	HANDLE hEvents[eventCount];

	// Main message loop
	while (WM_QUIT != msg.message)
	{
		hEvents[0] = m_hNextSkeletonEvent;

		// Check to see if we have either a message (by passing in QS_ALLEVENTS)
		// Or a Kinect event (hEvents)
		// Update() will check for Kinect events individually, in case more than one are signalled
		MsgWaitForMultipleObjects(eventCount, hEvents, FALSE, INFINITE, QS_ALLINPUT);

		// Explicitly check the Kinect frame event since MsgWaitForMultipleObjects
		// can return for other reasons even though it is signaled.
		Update();
		ProcessSpeech();
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// If a dialog message will be taken care of by the dialog proc
			if ((hWndApp != NULL) && IsDialogMessageW(hWndApp, &msg))
			{
				continue;
			}

			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	return static_cast<int>(msg.wParam);
}

/// <summary>
/// Main processing function
/// </summary>
void CSkeletonBasics::Update()
{
	if (NULL == m_pNuiSensor)
	{
		return;
	}

	// Wait for 0ms, just quickly test if it is time to process a skeleton
	if (WAIT_OBJECT_0 == WaitForSingleObject(m_hNextSkeletonEvent, 0))
	{
		ProcessSkeleton();
	}
}

/// <summary>
/// Handles window messages, passes most to the class instance to handle
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CSkeletonBasics::MessageRouter(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CSkeletonBasics* pThis = NULL;

	if (WM_INITDIALOG == uMsg)
	{
		pThis = reinterpret_cast<CSkeletonBasics*>(lParam);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
	}
	else
	{
		pThis = reinterpret_cast<CSkeletonBasics*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}

	if (pThis)
	{
		return pThis->DlgProc(hWnd, uMsg, wParam, lParam);
	}

	return 0;
}

/// <summary>
/// Handle windows messages for the class instance
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CSkeletonBasics::DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
						  // Bind application window handle
						  m_hWnd = hWnd;

						  // Init Direct2D
						  D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

						  // Look for a connected Kinect, and create it if found
						  CreateFirstConnected();
	}
		break;

		// If the titlebar X is clicked, destroy app
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;

	case WM_DESTROY:
		// Quit the main message pump
		PostQuitMessage(0);
		break;

		// Handle button press
	case WM_COMMAND:
		// If it was for the near mode control and a clicked event, change near mode
		if (IDC_CHECK_SEATED == LOWORD(wParam) && BN_CLICKED == HIWORD(wParam))
		{
			// Toggle out internal state for near mode
			m_bSeatedMode = !m_bSeatedMode;

			if (NULL != m_pNuiSensor)
			{
				// Set near mode for sensor based on our internal state
				m_pNuiSensor->NuiSkeletonTrackingEnable(m_hNextSkeletonEvent, m_bSeatedMode ? NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT : 0);
			}
		}
		break;
	}

	return FALSE;
}

/// <summary>
/// Create the first connected Kinect found 
/// </summary>
/// <returns>indicates success or failure</returns>
HRESULT CSkeletonBasics::CreateFirstConnected()
{
	INuiSensor * pNuiSensor;

	int iSensorCount = 0;
	HRESULT hr = NuiGetSensorCount(&iSensorCount);
	if (FAILED(hr))
	{
		return hr;
	}

	// Look at each Kinect sensor
	for (int i = 0; i < iSensorCount; ++i)
	{
		// Create the sensor so we can check status, if we can't create it, move on to the next
		hr = NuiCreateSensorByIndex(i, &pNuiSensor);
		if (FAILED(hr))
		{
			continue;
		}

		// Get the status of the sensor, and if connected, then we can initialize it
		hr = pNuiSensor->NuiStatus();
		if (S_OK == hr)
		{
			m_pNuiSensor = pNuiSensor;
			break;
		}

		// This sensor wasn't OK, so release it since we're not using it
		pNuiSensor->Release();
	}
	hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_AUDIO | NUI_INITIALIZE_FLAG_USES_SKELETON);
	if (NULL != m_pNuiSensor)
	{
		// Initialize the Kinect and specify that we'll be using skeleton

		if (SUCCEEDED(hr))
		{
			// Create an event that will be signaled when skeleton data is available

		}
	}

	if (NULL == m_pNuiSensor || FAILED(hr))
	{
		SetStatusMessage(L"No ready Kinect found!");
		return E_FAIL;
	}
	hr = InitializeAudioStream();

	if (FAILED(hr))
	{
		SetStatusMessage(L"Could not initialize audio stream.");
		return hr;
	}

	hr = CreateSpeechRecognizer();
	if (FAILED(hr))
	{
		SetStatusMessage(L"Could not create speech recognizer. Please ensure that Microsoft Speech SDK and other sample requirements are installed.");
		return hr;
	}

	hr = LoadSpeechGrammar();
	if (FAILED(hr))
	{
		SetStatusMessage(L"Could not load speech grammar. Please ensure that grammar configuration file was properly deployed.");
		return hr;
	}

	hr = StartSpeechRecognition();
	if (FAILED(hr))
	{
		SetStatusMessage(L"Could not start recognizing speech.");
		return hr;
	}

	m_hNextSkeletonEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

	// Open a skeleton stream to receive skeleton data
	hr = m_pNuiSensor->NuiSkeletonTrackingEnable(m_hNextSkeletonEvent, 0);
	return hr;
}

/// <summary>
/// Handle new skeleton data
/// </summary>
void CSkeletonBasics::ProcessSkeleton()
{
	NUI_SKELETON_FRAME skeletonFrame = { 0 };

	HRESULT hr = m_pNuiSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame);
	if (FAILED(hr))
	{
		return;
	}

	// smooth out the skeleton data
	m_pNuiSensor->NuiTransformSmooth(&skeletonFrame, NULL);

	// Endure Direct2D is ready to draw
	hr = EnsureDirect2DResources();
	if (FAILED(hr))
	{
		return;
	}

	m_pRenderTarget->BeginDraw();
	m_pRenderTarget->Clear();

	RECT rct;
	GetClientRect(GetDlgItem(m_hWnd, IDC_VIDEOVIEW), &rct);
	int width = rct.right;
	int height = rct.bottom;

	for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
	{
		NUI_SKELETON_TRACKING_STATE trackingState = skeletonFrame.SkeletonData[i].eTrackingState;

		if (NUI_SKELETON_TRACKED == trackingState)
		{
			// We're tracking the skeleton, draw 
			DrawSkeleton(skeletonFrame.SkeletonData[i], width, height);


			int x = skeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD].x * 1000;
			int y = skeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HEAD].y * 1000;
			int Lsy = skeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].y * 1000;
			int Rsy = skeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].y * 1000;
			if (count < 50)
			{
				centerX = x;
				centerY = y;
				leftshoulderY = Lsy;
				rightshoulderY = Rsy;
				count++;

			}
			else
			{
				Movemouse(x, y, Rsy, Lsy);

			}







		}
		else if (NUI_SKELETON_POSITION_ONLY == trackingState)
		{
			// we've only received the center point of the skeleton, draw that
			D2D1_ELLIPSE ellipse = D2D1::Ellipse(
				SkeletonToScreen(skeletonFrame.SkeletonData[i].Position, width, height),
				g_JointThickness,
				g_JointThickness
				);

			m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointTracked);
		}
	}

	hr = m_pRenderTarget->EndDraw();

	// Device lost, need to recreate the render target
	// We'll dispose it now and retry drawing
	if (D2DERR_RECREATE_TARGET == hr)
	{
		hr = S_OK;
		DiscardDirect2DResources();
	}
}

/// <summary>
/// Draws a skeleton
/// </summary>
/// <param name="skel">skeleton to draw</param>
/// <param name="windowWidth">width (in pixels) of output buffer</param>
/// <param name="windowHeight">height (in pixels) of output buffer</param>
void CSkeletonBasics::DrawSkeleton(const NUI_SKELETON_DATA & skel, int windowWidth, int windowHeight)
{
	int i;

	for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
	{
		m_Points[i] = SkeletonToScreen(skel.SkeletonPositions[i], windowWidth, windowHeight);
	}

	// Render Torso
	DrawBone(skel, NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER);
	DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT);
	DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT);
	DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE);
	DrawBone(skel, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER);
	DrawBone(skel, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT);
	DrawBone(skel, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT);

	// Left Arm
	DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT);
	DrawBone(skel, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT);
	DrawBone(skel, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT);

	// Right Arm
	DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT);
	DrawBone(skel, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT);
	DrawBone(skel, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT);

	// Left Leg
	DrawBone(skel, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT);
	DrawBone(skel, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT);
	DrawBone(skel, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT);

	// Right Leg
	DrawBone(skel, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT);
	DrawBone(skel, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT);
	DrawBone(skel, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT);

	// Draw the joints in a different color
	for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
	{
		D2D1_ELLIPSE ellipse = D2D1::Ellipse(m_Points[i], g_JointThickness, g_JointThickness);

		if (skel.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_INFERRED)
		{
			m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointInferred);
		}
		else if (skel.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_TRACKED)
		{
			m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointTracked);
		}
	}
}

/// <summary>
/// Draws a bone line between two joints
/// </summary>
/// <param name="skel">skeleton to draw bones from</param>
/// <param name="joint0">joint to start drawing from</param>
/// <param name="joint1">joint to end drawing at</param>
void CSkeletonBasics::DrawBone(const NUI_SKELETON_DATA & skel, NUI_SKELETON_POSITION_INDEX joint0, NUI_SKELETON_POSITION_INDEX joint1)
{
	NUI_SKELETON_POSITION_TRACKING_STATE joint0State = skel.eSkeletonPositionTrackingState[joint0];
	NUI_SKELETON_POSITION_TRACKING_STATE joint1State = skel.eSkeletonPositionTrackingState[joint1];

	// If we can't find either of these joints, exit
	if (joint0State == NUI_SKELETON_POSITION_NOT_TRACKED || joint1State == NUI_SKELETON_POSITION_NOT_TRACKED)
	{
		return;
	}

	// Don't draw if both points are inferred
	if (joint0State == NUI_SKELETON_POSITION_INFERRED && joint1State == NUI_SKELETON_POSITION_INFERRED)
	{
		return;
	}

	// We assume all drawn bones are inferred unless BOTH joints are tracked
	if (joint0State == NUI_SKELETON_POSITION_TRACKED && joint1State == NUI_SKELETON_POSITION_TRACKED)
	{
		m_pRenderTarget->DrawLine(m_Points[joint0], m_Points[joint1], m_pBrushBoneTracked, g_TrackedBoneThickness);
	}
	else
	{
		m_pRenderTarget->DrawLine(m_Points[joint0], m_Points[joint1], m_pBrushBoneInferred, g_InferredBoneThickness);
	}
}

/// <summary>
/// Converts a skeleton point to screen space
/// </summary>
/// <param name="skeletonPoint">skeleton point to tranform</param>
/// <param name="width">width (in pixels) of output buffer</param>
/// <param name="height">height (in pixels) of output buffer</param>
/// <returns>point in screen-space</returns>
D2D1_POINT_2F CSkeletonBasics::SkeletonToScreen(Vector4 skeletonPoint, int width, int height)
{
	LONG x, y;
	USHORT depth;

	// Calculate the skeleton's position on the screen
	// NuiTransformSkeletonToDepthImage returns coordinates in NUI_IMAGE_RESOLUTION_320x240 space
	NuiTransformSkeletonToDepthImage(skeletonPoint, &x, &y, &depth);

	float screenPointX = static_cast<float>(x * width) / cScreenWidth;
	float screenPointY = static_cast<float>(y * height) / cScreenHeight;

	return D2D1::Point2F(screenPointX, screenPointY);
}

/// <summary>
/// Ensure necessary Direct2d resources are created
/// </summary>
/// <returns>S_OK if successful, otherwise an error code</returns>
HRESULT CSkeletonBasics::EnsureDirect2DResources()
{
	HRESULT hr = S_OK;

	// If there isn't currently a render target, we need to create one
	if (NULL == m_pRenderTarget)
	{
		RECT rc;
		GetWindowRect(GetDlgItem(m_hWnd, IDC_VIDEOVIEW), &rc);

		int width = rc.right - rc.left;
		int height = rc.bottom - rc.top;
		D2D1_SIZE_U size = D2D1::SizeU(width, height);
		D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
		rtProps.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
		rtProps.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

		// Create a Hwnd render target, in order to render to the window set in initialize
		hr = m_pD2DFactory->CreateHwndRenderTarget(
			rtProps,
			D2D1::HwndRenderTargetProperties(GetDlgItem(m_hWnd, IDC_VIDEOVIEW), size),
			&m_pRenderTarget
			);
		if (FAILED(hr))
		{
			SetStatusMessage(L"Couldn't create Direct2D render target!");
			return hr;
		}

		//light green
		m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.27f, 0.75f, 0.27f), &m_pBrushJointTracked);

		m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow, 1.0f), &m_pBrushJointInferred);
		m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Green, 1.0f), &m_pBrushBoneTracked);
		m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Gray, 1.0f), &m_pBrushBoneInferred);
	}

	return hr;
}

/// <summary>
/// Dispose Direct2d resources 
/// </summary>
void CSkeletonBasics::DiscardDirect2DResources()
{
	SafeRelease(m_pRenderTarget);

	SafeRelease(m_pBrushJointTracked);
	SafeRelease(m_pBrushJointInferred);
	SafeRelease(m_pBrushBoneTracked);
	SafeRelease(m_pBrushBoneInferred);
}

/// <summary>
/// Set the status bar message
/// </summary>
/// <param name="szMessage">message to display</param>
HRESULT CSkeletonBasics::InitializeAudioStream()
{
	INuiAudioBeam*      pNuiAudioSource = NULL;
	IMediaObject*       pDMO = NULL;
	IPropertyStore*     pPropertyStore = NULL;
	IStream*            pStream = NULL;

	// Get the audio source
	HRESULT hr = m_pNuiSensor->NuiGetAudioSource(&pNuiAudioSource);
	if (SUCCEEDED(hr))
	{
		hr = pNuiAudioSource->QueryInterface(IID_IMediaObject, (void**)&pDMO);

		if (SUCCEEDED(hr))
		{
			hr = pNuiAudioSource->QueryInterface(IID_IPropertyStore, (void**)&pPropertyStore);

			// Set AEC-MicArray DMO system mode. This must be set for the DMO to work properly.
			// Possible values are:
			//   SINGLE_CHANNEL_AEC = 0
			//   OPTIBEAM_ARRAY_ONLY = 2
			//   OPTIBEAM_ARRAY_AND_AEC = 4
			//   SINGLE_CHANNEL_NSAGC = 5
			PROPVARIANT pvSysMode;
			PropVariantInit(&pvSysMode);
			pvSysMode.vt = VT_I4;
			pvSysMode.lVal = (LONG)(2); // Use OPTIBEAM_ARRAY_ONLY setting. Set OPTIBEAM_ARRAY_AND_AEC instead if you expect to have sound playing from speakers.
			pPropertyStore->SetValue(MFPKEY_WMAAECMA_SYSTEM_MODE, pvSysMode);
			PropVariantClear(&pvSysMode);

			// Set DMO output format
			WAVEFORMATEX wfxOut = { AudioFormat, AudioChannels, AudioSamplesPerSecond, AudioAverageBytesPerSecond, AudioBlockAlign, AudioBitsPerSample, 0 };
			DMO_MEDIA_TYPE mt = { 0 };
			MoInitMediaType(&mt, sizeof(WAVEFORMATEX));

			mt.majortype = MEDIATYPE_Audio;
			mt.subtype = MEDIASUBTYPE_PCM;
			mt.lSampleSize = 0;
			mt.bFixedSizeSamples = TRUE;
			mt.bTemporalCompression = FALSE;
			mt.formattype = FORMAT_WaveFormatEx;
			memcpy(mt.pbFormat, &wfxOut, sizeof(WAVEFORMATEX));

			hr = pDMO->SetOutputType(0, &mt, 0);

			if (SUCCEEDED(hr))
			{
				m_pKinectAudioStream = new KinectAudioStream(pDMO);

				hr = m_pKinectAudioStream->QueryInterface(IID_IStream, (void**)&pStream);

				if (SUCCEEDED(hr))
				{
					hr = CoCreateInstance(CLSID_SpStream, NULL, CLSCTX_INPROC_SERVER, __uuidof(ISpStream), (void**)&m_pSpeechStream);

					if (SUCCEEDED(hr))
					{
						hr = m_pSpeechStream->SetBaseStream(pStream, SPDFID_WaveFormatEx, &wfxOut);
					}
				}
			}

			MoFreeMediaType(&mt);
		}
	}

	SafeRelease(pStream);
	SafeRelease(pPropertyStore);
	SafeRelease(pDMO);
	SafeRelease(pNuiAudioSource);

	return hr;
}
HRESULT CSkeletonBasics::CreateSpeechRecognizer()
{
	ISpObjectToken *pEngineToken = NULL;

	HRESULT hr = CoCreateInstance(CLSID_SpInprocRecognizer, NULL, CLSCTX_INPROC_SERVER, __uuidof(ISpRecognizer), (void**)&m_pSpeechRecognizer);

	if (SUCCEEDED(hr))
	{
		m_pSpeechRecognizer->SetInput(m_pSpeechStream, FALSE);
		hr = SpFindBestToken(SPCAT_RECOGNIZERS, L"Language=409;Kinect=True", NULL, &pEngineToken);

		if (SUCCEEDED(hr))
		{
			m_pSpeechRecognizer->SetRecognizer(pEngineToken);
			hr = m_pSpeechRecognizer->CreateRecoContext(&m_pSpeechContext);

			// For long recognition sessions (a few hours or more), it may be beneficial to turn off adaptation of the acoustic model. 
			// This will prevent recognition accuracy from degrading over time.
			//if (SUCCEEDED(hr))
			//{
			//    hr = m_pSpeechRecognizer->SetPropertyNum(L"AdaptationOn", 0);                
			//}
		}
	}

	SafeRelease(pEngineToken);

	return hr;
}
HRESULT CSkeletonBasics::LoadSpeechGrammar()
{
	HRESULT hr = m_pSpeechContext->CreateGrammar(1, &m_pSpeechGrammar);

	if (SUCCEEDED(hr))
	{
		// Populate recognition grammar from file
		hr = m_pSpeechGrammar->LoadCmdFromFile(GrammarFileName, SPLO_STATIC);
	}

	return hr;
}
HRESULT CSkeletonBasics::StartSpeechRecognition()
{
	HRESULT hr = m_pKinectAudioStream->StartCapture();

	if (SUCCEEDED(hr))
	{
		// Specify that all top level rules in grammar are now active
		m_pSpeechGrammar->SetRuleState(NULL, NULL, SPRS_ACTIVE);

		// Specify that engine should always be reading audio
		m_pSpeechRecognizer->SetRecoState(SPRST_ACTIVE_ALWAYS);

		// Specify that we're only interested in receiving recognition events
		m_pSpeechContext->SetInterest(SPFEI(SPEI_RECOGNITION), SPFEI(SPEI_RECOGNITION));

		// Ensure that engine is recognizing speech and not in paused state
		hr = m_pSpeechContext->Resume(0);
		if (SUCCEEDED(hr))
		{
			m_hSpeechEvent = m_pSpeechContext->GetNotifyEventHandle();
		}
	}

	return hr;
}
void CSkeletonBasics::ProcessSpeech()
{
	const float ConfidenceThreshold = 0.3f;

	SPEVENT curEvent;
	ULONG fetched = 0;
	HRESULT hr = S_OK;

	m_pSpeechContext->GetEvents(1, &curEvent, &fetched);

	while (fetched > 0)
	{
		switch (curEvent.eEventId)
		{
		case SPEI_RECOGNITION:
			if (SPET_LPARAM_IS_OBJECT == curEvent.elParamType)
			{
				// this is an ISpRecoResult
				ISpRecoResult* result = reinterpret_cast<ISpRecoResult*>(curEvent.lParam);
				SPPHRASE* pPhrase = NULL;
				hr = result->GetPhrase(&pPhrase);
				if (SUCCEEDED(hr))
				{

					if ((pPhrase->pProperties != NULL) && (pPhrase->pProperties->pFirstChild != NULL))
					{
						const SPPHRASEPROPERTY* pSemanticTag = pPhrase->pProperties->pFirstChild;
						if (pSemanticTag->SREngineConfidence > ConfidenceThreshold)
						{
							//things happen
							ProgramActions action = MapSpeechTagToAction(pSemanticTag->pszValue);
							DoAction(action);
						}
					}
					::CoTaskMemFree(pPhrase);
				}
			}
			break;
		}

		m_pSpeechContext->GetEvents(1, &curEvent, &fetched);
	}

	return;
}



ProgramActions CSkeletonBasics::MapSpeechTagToAction(LPCWSTR pszSpeechTag)
{
	//SendDlgItemMessageW(m_hWnd, IDC_STATUS, WM_SETTEXT, 0,(LPARAM) pszSpeechTag);
	struct SpeechTagToAction
	{
		LPCWSTR pszSpeechTag;
		ProgramActions action;
	};
	const SpeechTagToAction Map[] =
	{
		{ L"CLICK", LeftClick },
		{ L"RIGHTCLICK", RightClick },
		{ L"A", A },
		{ L"B", B },
		{ L"C", C },
		{ L"D", D },
		{ L"E", E },
		{ L"F", F },
		{ L"G", G },
		{ L"H", H },
		{ L"I", I },
		{ L"J", J },
		{ L"K", K },
		{ L"L", L },
		{ L"M", M },
		{ L"N", N },
		{ L"O", O },
		{ L"P", P },
		{ L"Q", Q },
		{ L"R", R },
		{ L"S", S },
		{ L"T", T },
		{ L"U", U },
		{ L"V", V },
		{ L"W", W },
		{ L"X", X },
		{ L"Y", Y },
		{ L"Z", Z },
	

		

	};

	ProgramActions action = NoAction;

	for (int i = 0; i < _countof(Map); ++i)
	{
		if (0 == wcscmp(Map[i].pszSpeechTag, pszSpeechTag))
		{
			action = Map[i].action;
			break;
		}
	}

	return action;
}

void CSkeletonBasics::DoAction(ProgramActions action)
{
	switch (action)
	{
	case RightClick:
		mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
		SetStatusMessage(L"Right Clicking");
		break;
	case LeftClick:
		mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
		SetStatusMessage(L"Left Clicking");
		break;
	case A:
		SetStatusMessage(L"A");
		enterkey(97);
		break;
	case B:
		SetStatusMessage(L"B");
		enterkey(98);
		break;
	case C:
		SetStatusMessage(L"C");
		enterkey(99);
		break;
	case D:
		SetStatusMessage(L"D");
		enterkey(100);
		break;
	case E:
		SetStatusMessage(L"E");
		enterkey(101);
		break;
	case F:
		SetStatusMessage(L"F");
		enterkey(102);
		break;
	case G:
		SetStatusMessage(L"G");
		enterkey(103);
		break;
	case H:
		SetStatusMessage(L"H");
		enterkey(104);
		break;
	case I:
		SetStatusMessage(L"I");
		enterkey(105);
		break;
	case J:
		SetStatusMessage(L"J");
		enterkey(106);
		break;
	case K:
		SetStatusMessage(L"K");
		enterkey(107);
		break;
	case L:
		SetStatusMessage(L"L");
		enterkey(108);
		break;
	case M:
		SetStatusMessage(L"M");
		enterkey(109);
		break;
	case N:
		SetStatusMessage(L"N");
		enterkey(110);
		break;
	case O:
		SetStatusMessage(L"O");
		enterkey(111);
		break;
	case P:
		SetStatusMessage(L"P");
		enterkey(112);
		break;
	case Q:
		SetStatusMessage(L"N");
		enterkey(113);
		break;
	case R:
		SetStatusMessage(L"R");
		enterkey(114);
		break;
	case S:
		SetStatusMessage(L"S");
		enterkey(115);
		break;
	case T:
		SetStatusMessage(L"T");
		enterkey(116);
		break;
	case U:
		SetStatusMessage(L"U");
		enterkey(117);
		break;
	case V:
		SetStatusMessage(L"V");
		enterkey(118);
		break;
	case W:
		SetStatusMessage(L"W");
		enterkey(119);
		break;
	case X:
		SetStatusMessage(L"X");
		enterkey(120);
		break;
	case Y:
		SetStatusMessage(L"Y");
		enterkey(121);
		break;
	case Z:
		SetStatusMessage(L"Z");
		enterkey(122);
		break;
	case NoAction:
		break;
	default:
		break;
	}
	// different actions goes here

}
void CSkeletonBasics::SetStatusMessage(WCHAR * szMessage)
{
	SendDlgItemMessageW(m_hWnd, IDC_STATUS, WM_SETTEXT, 0, (LPARAM)szMessage);
}

void Movemouse(int x, int y, int Rsy, int Lsy)
{
	POINT cursorLoc;
	//get mouse position
	GetCursorPos(&cursorLoc);
	ShowCursor(0);


	// this is were we move the mouse based on the changes of the kinect we need to tweak
	int speed = 3;
	int difx = x - centerX;
	int dify = y - centerY;
	if (difx >10)
	{
		cursorLoc.x = cursorLoc.x + speed;

	}

	if (difx < -10)
	{
		cursorLoc.x = cursorLoc.x - speed;
	}


	if (dify >3)
	{
		cursorLoc.y = cursorLoc.y - speed;

	}

	if (dify < -15)
	{
		cursorLoc.y = cursorLoc.y + speed;
	}
	SetCursorPos(cursorLoc.x, cursorLoc.y);

}

void enterkey(char keyinput){
	INPUT input; // INPUT structure
	memset(&input, 0, sizeof(input));

	input.type = INPUT_KEYBOARD;

	input.ki.wVk = VkKeyScanA(keyinput);

	//  Sleep(1000);

	SendInput(1, &input, sizeof(INPUT)); 
	input.ki.dwFlags = KEYEVENTF_KEYUP;

	SendInput(1, &input, sizeof(INPUT));

}