#pragma once

#include <wx/glcanvas.h>
#include <wx/wx.h>
#include <wx/hyperlink.h>
#include <wx/aboutdlg.h>


#include <glm/glm.hpp>


// the rendering context used by all GL canvases
class TestGLContext : public wxGLContext
{
public:
	TestGLContext(wxGLCanvas *canvas);

	// render the cube showing it at given angles
	void DrawRotatedCube(float xangle = 0, float yangle = 0);

	void DrawRotatedCube(glm::fquat q);

	void DrawRotatedCube(float xangle = 0, float yangle = 0, float zangle = 0);

private:
	// textures for the cube faces
	GLuint m_textures[6];
};


class TestGLCanvas : public wxGLCanvas
{
public:
	TestGLCanvas(wxWindow *parent, int *attribList = NULL);

	// angles of rotation around x- and y- axis
	float m_xangle;
	float m_yangle;

private:
	void OnPaint(wxPaintEvent& event);
	void OnKeyDown(wxKeyEvent& event);
	void OnSpinTimer(wxTimerEvent& WXUNUSED(event));



	wxTimer m_spinTimer;
	bool m_useStereo,
		m_stereoWarningAlreadyDisplayed;

	wxDECLARE_EVENT_TABLE();
};

enum
{
	NEW_STEREO_WINDOW = wxID_HIGHEST + 1
};


// Define a new application type
class MyApp : public wxApp
{
public:
	MyApp() {};

    // virtual wxApp methods
    virtual bool OnInit();

	virtual int OnExit();

	void onIdle(wxIdleEvent &evt);

	// Returns the shared context used by all frames and sets it as current for
	// the given canvas.
	TestGLContext& GetContext(wxGLCanvas *canvas, bool useStereo);

private:
	// the GL context we use for all our mono rendering windows
	TestGLContext *m_glContext;
	// the GL context we use for all our stereo rendering windows
	TestGLContext *m_glStereoContext;

	wxTimer m_myTimer;
};




// Define a new frame type
class MainFrame : public wxFrame
{
public:

	wxApp *parent;


	wxCheckBox* CB1;
	wxCheckBox* CB2;
	wxCheckBox* CB3;
	wxCheckBox* CB4;
	wxCheckBox* CB5;
	wxCheckBox* CB6;
	wxCheckBox* CB7;
	wxCheckBox* CB8;
	wxCheckBox* CB9;
	wxCheckBox* CB10;
	wxCheckBox* CB11;
	wxCheckBox* CB12;
	wxCheckBox* CB13;
	wxCheckBox* CB14;
	wxCheckBox* CB15;
	wxCheckBox* CB16;



	wxStaticText* st1;
	wxStaticText* st2;
	wxStaticText* st3;
	wxStaticText* st4;

	wxSlider* slider1;
	wxSlider* slider2;
	wxStaticText* slider1Text;
	wxStaticText* slider2Text;


	wxButton* startButton;
	wxButton* quitButton;
	wxButton* updateButton;
	wxButton* donateButton;


	MainFrame();

	void onStart(wxCommandEvent&);
	void onQuit(wxCommandEvent&);
	
	void onQuit2(wxCloseEvent&);

	void onUpdate(wxCommandEvent&);
	void onDonate(wxCommandEvent &);

	void toggleCombine(wxCommandEvent&);

	void toggleGyro(wxCommandEvent&);
	void toggleGyroWindow(wxCommandEvent &);
	void toggleMario(wxCommandEvent&);

	void toggleReverseX(wxCommandEvent&);
	void toggleReverseY(wxCommandEvent&);

	void togglePreferLeftJoyCon(wxCommandEvent&);
	void toggleQuickToggleGyro(wxCommandEvent&);
	void toggleInvertQuickToggle(wxCommandEvent&);
	void toggleDolphinPointerMode(wxCommandEvent&);

	void toggleDebugMode(wxCommandEvent&);
	void toggleWriteDebug(wxCommandEvent&);

	void toggleForcePoll(wxCommandEvent&);

	void setGyroSensitivityX(wxCommandEvent&);
	void setGyroSensitivityY(wxCommandEvent&);


	void toggleBroadcastMode(wxCommandEvent&);
	void toggleWriteCastToFile(wxCommandEvent&);


	void checkForUpdate();

};






// Define a new frame type
class MyFrame : public wxFrame
{
public:
	MyFrame(bool stereoWindow = false);

private:
	void OnClose(wxCommandEvent& event);
	void OnNewWindow(wxCommandEvent& event);
	void OnNewStereoWindow(wxCommandEvent& event);

	wxDECLARE_EVENT_TABLE();
};




