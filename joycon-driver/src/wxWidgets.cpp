#include <wx/wx.h>

class app : public wxApp {
public:
	bool OnInit() {
		wxFrame* window = new wxFrame(nullptr, -1, "test");
		window->Show();
		return true;
	}
};

IMPLEMENT_APP(app);