#include "GacLib\GacUI.h"
#include "GacLib\GacUIWindows.h"
#include <stdio.h>
#pragma warning(disable:4244)

LRESULT CALLBACK MessageWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int CmdShow)
{
	// create a message only window to handle system window messages
	static const wchar_t* className = L"MessageHandleWindow";

	HWND messageOnlyWindow;
	WNDCLASSEX wx = {};
	wx.cbSize = sizeof(WNDCLASSEX);
	wx.lpfnWndProc = MessageWindowProc;
	wx.hInstance = hInstance;
	wx.lpszClassName = className;
	if (RegisterClassEx(&wx))
	{
		messageOnlyWindow = CreateWindowEx(0, className, L"MessageWnd", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
	}

	// start gacui
	return SetupWindowsDirect2DRenderer();
}


class OscilloscopeMainWindow : public GuiWindow
{
private:
	GuiToolstripMenuBar*			m_menuBar;
	GuiDirect2DElement*				m_OscilloscopeScreen;

	GuiToolstripButton*				m_ButtonStart;
	ComPtr<ID2D1SolidColorBrush>	m_TestBrush;
	ComPtr<IDWriteTextFormat>		m_defaultTextFormat;
	ComPtr<ID2D1SolidColorBrush>	m_defaultTextBrush;

	GuiToolstripCommand*				m_commandDebugShowFPS;
	collections::List<GuiToolstripCommand*>		m_portsCommand;
	collections::List<WString>					m_availableSerialPorts;

	Thread*							m_checkPortsThread;
	Thread*							m_readPortThread;


	//////////////////////////////////////////////////////////////
	//				      Internal States						//
	//////////////////////////////////////////////////////////////
	bool m_showFPS;

public:
	OscilloscopeMainWindow()
		: GuiWindow(GetCurrentTheme()->CreateWindowStyle())
	{
		// Initial internal states
		m_showFPS = false;

		// Initial windows elements and layout
		this->SetText(L"Oscilloscope V0.01");
		this->SetClientSize(Size(800, 600));

		InitializeCommands();

		GuiTableComposition* table = new GuiTableComposition;
		table->SetCellPadding(0);
		table->SetAlignmentToParent(Margin(0, 0, 0, 0));
		table->SetRowsAndColumns(2, 2);
		table->SetRowOption(0, GuiCellOption::MinSizeOption());
		table->SetRowOption(1, GuiCellOption::PercentageOption(1.0));
		table->SetColumnOption(0, GuiCellOption::MinSizeOption());
		table->SetColumnOption(1, GuiCellOption::PercentageOption(1.0));
		this->GetContainerComposition()->AddChild(table);

		{
			GuiCellComposition* cell = new GuiCellComposition;
			table->AddChild(cell);
			cell->SetSite(0, 0, 1, 2);
			m_menuBar = g::NewMenuBar();
			m_menuBar->GetBoundsComposition()->SetMinSizeLimitation(GuiGraphicsComposition::LimitToElementAndChildren);
			m_menuBar->GetBoundsComposition()->SetAlignmentToParent(Margin(0, 0, 0, 0));

			// set menu
			m_menuBar->GetBuilder()
				->Button(0, L"File")
				->BeginSubMenu()
				->Splitter()
				->EndSubMenu()
				->Button(0, L"View")
				->BeginSubMenu()
				->Splitter()
				->EndSubMenu()
				->Button(0, L"Input")
				->BeginSubMenu()
				->Button(0, L"Serial Port")
				->EndSubMenu()
				->Button(0, L"Debug")
				->BeginSubMenu()
				->Button(m_commandDebugShowFPS)
				->EndSubMenu();
			cell->AddChild(m_menuBar->GetBoundsComposition());
		}

		// Display
		{
			GuiCellComposition* cell = new GuiCellComposition;
			table->AddChild(cell);
			cell->SetSite(1, 0, 1, 1);
			cell->SetInternalMargin(Margin(1, 0, 1, 0));
			cell->SetMinSizeLimitation(GuiGraphicsComposition::LimitToElementAndChildren);

			GuiDirect2DElement* element = GuiDirect2DElement::Create();
			element->Rendering.AttachMethod(this, &OscilloscopeMainWindow::OnRendering);
			element->BeforeRenderTargetChanged.AttachMethod(this, &OscilloscopeMainWindow::OnRenderTargetLost);
			element->AfterRenderTargetChanged.AttachMethod(this, &OscilloscopeMainWindow::OnRenderTargetGet);

			GuiBoundsComposition* composition = new GuiBoundsComposition;
			composition->SetAlignmentToParent(Margin(0, 0, 0, 0));
			composition->SetBounds(Rect(0, 0, 640, 640));
			composition->SetOwnedElement(element);
			cell->AddChild(composition);
		}

		// Controll Panel
		{
			GuiCellComposition* cell = new GuiCellComposition;
			table->AddChild(cell);
			cell->SetSite(1, 1, 1, 1);
			cell->SetInternalMargin(Margin(1, 0, 1, 0));

			//GuiTableComposition* table = new GuiTableComposition;
			m_ButtonStart = g::NewToolBarButton();
			m_ButtonStart->SetText(L"Start");
			m_ButtonStart->SetAutoSelection(true);
			GuiBoundsComposition* buttonComposition = m_ButtonStart->GetBoundsComposition();
			buttonComposition->SetAlignmentToParent(Margin(5, 5, 5, 5));
			cell->AddChild(m_ButtonStart->GetBoundsComposition());
		}

		// Create some threads to peek and read serial ports
		m_checkPortsThread = Thread::CreateAndStart([=](){CheckSerialPorts(); });
		m_readPortThread = Thread::CreateAndStart([=](){ReadSerialPort(); });
	}

	void OnRendering(GuiGraphicsComposition* sender, GuiDirect2DElementEventArgs& arguments)
	{
		int SurfaceWidth = arguments.bounds.Width();
		int SurfaceHeight = arguments.bounds.Height();
		int SurfaceX = arguments.bounds.Left();
		int SurfaceY = arguments.bounds.Top();

		ID2D1RenderTarget* renderTarget = arguments.rt;
		renderTarget->Clear(D2D1::ColorF(0, 0, 0));

		// Insert a code slice to calculate FPS....
		{
			static int lastTick = ::GetTickCount();
			static int frames = 0;
			static int fps = 0;
			if (m_showFPS)
			{
				int currentTick = ::GetTickCount();
				if (currentTick - lastTick > 1000)
				{
					fps = frames / float(currentTick - lastTick) * 1000.0f;
					lastTick = currentTick;
					frames = 0;
				}
				frames++;
				WString fpsString = L"FPS = " + vl::itow(fps);
				renderTarget->DrawTextW(fpsString.Buffer(), fpsString.Length(), m_defaultTextFormat.Obj(), D2D1::RectF(5.0f + SurfaceX, 5.0f + SurfaceY, 100.0f + SurfaceX, 25.0f + SurfaceY), m_TestBrush.Obj());
			}
		}

		// Draw Background Grid
		float strokeWidth = 0.5f;
		{
			renderTarget->DrawLine(D2D1::Point2F(SurfaceWidth / 2 + SurfaceX, 0 + SurfaceY), D2D1::Point2F(SurfaceWidth / 2 + SurfaceX, SurfaceHeight + SurfaceY), m_TestBrush.Obj(), strokeWidth);

			float Y = SurfaceY;
			for (int i = 0; i < 100; i++)
			{
				if (i == 50)
				{
					renderTarget->DrawLine(D2D1::Point2F(0 + SurfaceX, Y), D2D1::Point2F(SurfaceWidth + SurfaceX, Y), m_TestBrush.Obj(), strokeWidth);
				}
				else if (i % 10 == 0)
				{
					renderTarget->DrawLine(D2D1::Point2F(SurfaceWidth / 2 + SurfaceX, Y), D2D1::Point2F(SurfaceWidth / 2 + SurfaceX + 10, Y), m_TestBrush.Obj(), strokeWidth);
				}
				else
				{
					renderTarget->DrawLine(D2D1::Point2F(SurfaceWidth / 2 + SurfaceX, Y), D2D1::Point2F(SurfaceWidth / 2 + SurfaceX + 3, Y), m_TestBrush.Obj(), strokeWidth);
				}
				Y += SurfaceHeight / 100.0f;
			}
		}
	}

	void OnRenderTargetGet(GuiGraphicsComposition* sender, GuiDirect2DElementEventArgs& arguments)
	{
		ID2D1SolidColorBrush* brush;
		arguments.rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 1.0f, 0.0f), D2D1::BrushProperties(), &brush);
		m_TestBrush = brush;

		IDWriteTextFormat* format;
		arguments.factoryDWrite->CreateTextFormat(
			font.fontFamily.Buffer(),
			NULL,
			(font.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL),
			(font.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL),
			DWRITE_FONT_STRETCH_NORMAL,
			(FLOAT) font.size,
			L"",
			&format);
		m_defaultTextFormat = format;
	}

	void OnRenderTargetLost(GuiGraphicsComposition* sender, GuiDirect2DElementEventArgs& arguments)
	{
		m_TestBrush = NULL;
		m_defaultTextFormat = NULL;
	}

	//////////////////////////////////////////////////////////
	//				Menu Processing Funcs					//
	//////////////////////////////////////////////////////////

	void OnDebugShowFps(GuiGraphicsComposition* sender, GuiEventArgs& arguments)
	{
		m_showFPS = !m_showFPS;
	}

	void OnSelectSerialPort(GuiGraphicsComposition* sender, GuiEventArgs& arguments)
	{

	}


	void InitializeCommands()
	{
		{
			m_commandDebugShowFPS = new GuiToolstripCommand;
			m_commandDebugShowFPS->SetText(L"ShowFPS");
			this->AddComponent(m_commandDebugShowFPS);
		}

		// Install processing handler
		m_commandDebugShowFPS->Executed.AttachMethod(this, &OscilloscopeMainWindow::OnDebugShowFps);
	}

	//////////////////////////////////////////////////////////
	//				Thread Function							//
	//////////////////////////////////////////////////////////
	void CheckSerialPorts()
	{
		// function only be called when system device changed
		// test serial port change, now only test 1-10
		bool dirtyMenu = false;
		for (int i = 1; i < 11; i++)
		{
			WString portName = WString(L"COM") + vl::itow(i);
			HANDLE returnValue = CreateFileW(portName.Buffer(), GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (returnValue != INVALID_HANDLE_VALUE)		// port is available
			{
				CloseHandle(returnValue);
				if (!m_availableSerialPorts.Contains(portName))
				{
					m_availableSerialPorts.Add(portName);
					dirtyMenu = true;
				}
			}
			else
			{
				if (m_availableSerialPorts.Contains(portName))
				{
					m_availableSerialPorts.Remove(portName);
					dirtyMenu = true;
				}
			}
		}

		// tell GUI to update menu
		if (dirtyMenu)
		{

			GetApplication()->InvokeInMainThreadAndWait([=]()
			{
				GuiToolstripButton* inputButton = reinterpret_cast<GuiToolstripButton*>(m_menuBar->GetToolstripItems().Get(2));								// 2 is third first level menu....
				GuiToolstripButton* serialInputButton = reinterpret_cast<GuiToolstripButton*>(inputButton->GetToolstripSubMenu()->GetToolstripItems().Get(0));	// the serial button should be first in input...
				serialInputButton->DestroySubMenu();
				serialInputButton->CreateToolstripSubMenu();
				for (int i = 0; i < m_availableSerialPorts.Count(); i++)
				{
					WString portName = m_availableSerialPorts[i];
					GuiToolstripCommand* portCommand = new GuiToolstripCommand;
					portCommand->SetText(portName);
					portCommand->Executed.AttachMethod(this, &OscilloscopeMainWindow::OnSelectSerialPort);
					GuiToolstripButton* portButton = new GuiToolstripButton(GetCurrentTheme()->CreateMenuItemButtonStyle());
					portButton->SetCommand(portCommand);
					serialInputButton->GetToolstripSubMenu()->GetBuilder()->Button(portCommand);
				}
			});
		}
	}

	void ReadSerialPort()
	{

	}

};


void GuiMain()
{
	GuiWindow* window = new OscilloscopeMainWindow;
	GetApplication()->Run(window);
	delete window;
}

//////////////////////////////////////////////////////////
//				Windows Message Handling				//
//////////////////////////////////////////////////////////
LRESULT CALLBACK MessageWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_DEVICECHANGE)
	{
		OscilloscopeMainWindow* mainWindow = reinterpret_cast<OscilloscopeMainWindow*>(GetApplication()->GetMainWindow());
		mainWindow->CheckSerialPorts();
	}
	return DefWindowProc(hWnd, Msg, wParam, lParam);
}