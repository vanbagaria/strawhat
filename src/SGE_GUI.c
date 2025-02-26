#include "SGE.h"
#include "SGE_GUI.h"
#include "SGE_Logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Engine data pointer */
static SGE_EngineData *engine = NULL;

/* Default GUI Fonts */
static TTF_Font *buttonFont = NULL;
static TTF_Font *panelTitleFont = NULL;
static TTF_Font *labelFont = NULL;
static TTF_Font *textBoxFont = NULL;
static TTF_Font *listBoxFont = NULL;

/* List for current state's controls */
static SGE_GUI_ControlList *currentStateControls = NULL;

/* 
* List for debug state's controls.
* The debug state is a set of controls that overlay on top of the current engine state.
* These controls include labels with FPS information, the debug panel and more.
* The debug state is toggled with the F1 key.
*/
static SGE_GUI_ControlList debugStateControls;

/* Toggle to disable debug state */
static bool showDebugState = false;

/* The debug panel, toggled with F2 */
static SGE_WindowPanel *debugPanel;

/* Toggle for displaying control boundboxes */
static bool showControlBounds = false;
static SGE_CheckBox *showBoundsChkBox;
static SGE_TextLabel *showBoundsLabel;
static SDL_Color controlBoundsColor;

/* Toggle for displaying frame rate information */
static bool showFrameInfo = true;
static SGE_CheckBox *showFrameInfoChkBox;
static SGE_TextLabel *showFrameInfoLbl;

/* List of panels in current state as a string */
static char panelsListStr[200];

static SGE_TextLabel *panelListLabel;
static SGE_TextLabel *stateListLabel;

/* Frame rate counter */
static int frameCounter;
static int countedFPS;
static int lastFPSCountTime;
//static int frameCountInterval = 500; // in ms

/* Frame info labels */
static char deltaStr[10];
static SGE_TextLabel *deltaLabel;
static char fpsStr[10];
static SGE_TextLabel *fpsLabel;
static char vsyncStr[10];
static SGE_TextLabel *vsyncLabel;

/* Generalization of GUI control lists */
static void SGE_GUI_ControlList_HandleEvents(SGE_GUI_ControlList *controls);
static void SGE_GUI_ControlList_Update(SGE_GUI_ControlList *controls);
static void SGE_GUI_ControlList_Render(SGE_GUI_ControlList *controls);
static void SGE_GUI_FreeControlList(SGE_GUI_ControlList *controls);

/* Handler for frame info labels toggle */
static void onShowFrameInfoToggle(void *data)
{
	if(!showFrameInfoChkBox->isChecked)
	{
		SGE_TextLabelSetText(deltaLabel, " ");
		SGE_TextLabelSetText(fpsLabel, " ");
		SGE_TextLabelSetText(vsyncLabel, " ");
		deltaLabel->showBG = false;
		fpsLabel->showBG = false;
		vsyncLabel->showBG = false;
	}
	else
	{
		deltaLabel->showBG = true;
		fpsLabel->showBG = true;
		vsyncLabel->showBG = true;
	}
}

static void SGE_GUI_DebugState_Init()
{
	SGE_GUI_ControlList *tempCurrentStateControls = currentStateControls;
	currentStateControls = &debugStateControls;
	
	/* Create the debug panel */
	debugPanel = SGE_CreateWindowPanel("Debug Panel", 0, 0, 320, 240);
	debugPanel->isVisible = false;
	//debugPanel->borderColor = SGE_COLOR_GRAY;
	debugPanel->alpha = 200;
	//debugPanel->backgroundColor = SGE_COLOR_GRAY;
	//debugPanel->borderColor = SGE_COLOR_BLACK;
	debugPanel->isMovable = false;
	debugPanel->isMinimizable = false;
	debugPanel->isResizable = false;
	SGE_WindowPanelSetPosition(debugPanel, engine->screenWidth - debugPanel->boundBox.w, engine->screenHeight - debugPanel->boundBox.h);

	/* Create panel and state list labels */
	panelListLabel = SGE_CreateTextLabel(panelsListStr, 10, 10, SGE_COLOR_WHITE, debugPanel);
	panelListLabel->mode = SGE_TEXT_MODE_SHADED;
	stateListLabel = SGE_CreateTextLabel(" ", 10, 35, SGE_COLOR_WHITE, debugPanel);
	stateListLabel->mode = SGE_TEXT_MODE_SHADED;

	/* Create toggles */
	showBoundsLabel = SGE_CreateTextLabel("Show Bound Boxes:", 0, 0, SGE_COLOR_BLACK, debugPanel);
	SGE_TextLabelSetPositionNextTo(showBoundsLabel, stateListLabel->boundBox, SGE_CONTROL_DIRECTION_DOWN, 0, 10);
	showBoundsChkBox = SGE_CreateCheckBox(0, 0, debugPanel);
	SGE_CheckBoxSetPositionNextTo(showBoundsChkBox, showBoundsLabel->boundBox, SGE_CONTROL_DIRECTION_RIGHT_CENTERED, 15, 0);
	
	showFrameInfoLbl = SGE_CreateTextLabel("Show Frame Info:", 0, 0, SGE_COLOR_BLACK, debugPanel);
	SGE_TextLabelSetPositionNextTo(showFrameInfoLbl, showBoundsLabel->boundBox, SGE_CONTROL_DIRECTION_DOWN, 0, 10);
	showFrameInfoChkBox = SGE_CreateCheckBox(0, 0, debugPanel);
	SGE_CheckBoxSetPositionNextTo(showFrameInfoChkBox, showFrameInfoLbl->boundBox, SGE_CONTROL_DIRECTION_RIGHT_CENTERED, 15, 0);
	showFrameInfoChkBox->onMouseUp = onShowFrameInfoToggle;
	showFrameInfoChkBox->isChecked = true;

	/* Create frame info labels */
	deltaLabel = SGE_CreateTextLabel(" ", 0, 0, SGE_COLOR_WHITE, NULL);
	SGE_TextLabelSetPosition(deltaLabel, 0, engine->screenHeight - deltaLabel->boundBox.h);
	SGE_TextLabelSetMode(deltaLabel, SGE_TEXT_MODE_SHADED);
	SGE_TextLabelSetBGColor(deltaLabel, SGE_COLOR_BLACK);

	fpsLabel = SGE_CreateTextLabel(" ", 0, 0, SGE_COLOR_WHITE, NULL);
	SGE_TextLabelSetPositionNextTo(fpsLabel, deltaLabel->boundBox, SGE_CONTROL_DIRECTION_UP, 0, 0);
	SGE_TextLabelSetMode(fpsLabel, SGE_TEXT_MODE_SHADED);
	SGE_TextLabelSetBGColor(fpsLabel, SGE_COLOR_BLACK);

	vsyncLabel = SGE_CreateTextLabel(" ", 0, 0, SGE_COLOR_WHITE, NULL);
	SGE_TextLabelSetPositionNextTo(vsyncLabel, fpsLabel->boundBox, SGE_CONTROL_DIRECTION_UP, 0, 0);
	SGE_TextLabelSetMode(vsyncLabel, SGE_TEXT_MODE_SHADED);
	SGE_TextLabelSetBGColor(vsyncLabel, SGE_COLOR_BLACK);

	currentStateControls = tempCurrentStateControls;
}

static void SGE_GUI_DebugState_Update()
{
	/* Sync toggles with checkboxes */
	showControlBounds = showBoundsChkBox->isChecked;
	showFrameInfo = showFrameInfoChkBox->isChecked;

	/* Update frame info labels */
	if(showFrameInfo)
	{
		sprintf(deltaStr, "dt: %.3f s", engine->delta);
		SGE_TextLabelSetText(deltaLabel, deltaStr);

		/* Calculate the framerate */
		frameCounter++;
		if((SDL_GetTicks() - lastFPSCountTime) > 1000)
		{
			countedFPS = frameCounter;
			frameCounter = 0;
			lastFPSCountTime = SDL_GetTicks();
		}
		sprintf(fpsStr, "fps: %d", countedFPS);
		SGE_TextLabelSetText(fpsLabel, fpsStr);

		if(engine->isVsyncOn) {
			sprintf(vsyncStr, "vsync: on");
		}
		else {
			sprintf(vsyncStr, "vsync: off");
		}
		SGE_TextLabelSetText(vsyncLabel, vsyncStr);
	}
}

char *SGE_GetPanelListAsStr()
{
	return panelsListStr;
}

/* Prints a list of all panels in the current state onto a string */
static void printPanelsStr()
{
	int i = 0;
	char tempStr[100];

	SGE_WindowPanel **panels = currentStateControls->panels;
	int panelCount = currentStateControls->panelCount;

	if(panelCount > 0)
	{
		/* Don't update the panel list string if handling debug state controls */
		if(strcmp(panels[0]->titleStr, "Debug Panel") == 0)
		{
			return;
		}
		
		/* For more than one panels */
		if(panelCount > 1)
		{
			sprintf(panelsListStr, "Panels: {%s: %s, ", panels[0]->titleStr, panels[0]->isActive ? "Active" : "Inactive");
			for(i = 1; i < panelCount - 1; i++)
			{
				sprintf(tempStr, "%s: %s, ", panels[i]->titleStr, panels[i]->isActive ? "Active" : "Inactive");
				strcat(panelsListStr, tempStr);
			}
			sprintf(tempStr, "%s: %s}", panels[panelCount - 1]->titleStr, panels[panelCount - 1]->isActive ? "Active" : "Inactive");
			strcat(panelsListStr, tempStr);
		}
		else /* For single panel */
		{
			sprintf(panelsListStr, "Panels: {%s: %s}", panels[0]->titleStr, panels[0]->isActive ? "Active" : "Inactive");
		}
	}
	else /* For no panels */
	{
		sprintf(panelsListStr, "Panels: {}");
	}
}

/* Wrapper for SGE_LogPrintLine() */
static void SGE_GUI_LogPrintLine(SGE_LogLevel level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	SGE_LogPrintLineCat(level, "GUI: ", format, &args);
	va_end(args);
}

/* Fallback handlers for GUI control events */

static void onDownFallbackCallback(void *data)
{
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "onDownFallback Called!");
}

static void onUpFallbackCallback(void *data)
{
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "onUpFallback Called!");
}

static void onSlideFallback(void *data)
{
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "onSlideCallback Called!");
}

static void onResizeFallback(void *data)
{
	// SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "onResizeCallback Called!");
}

static void onMoveFallback(void *data)
{
	// SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "onMoveCallback Called!");
}

static void onMinimizeFallback(void *data)
{
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "onMinimizeCallback Called!");
}

static void onMaximizeFallback(void *data)
{
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "onMaximizeCallback Called!");
}

static void onEnableFallback(void *data)
{
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "onEnableCallback Called!");
}

static void onDisableFallback(void *data)
{
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "onDisableCallback Called!");
}

static void onTextEnterFallback(void *data)
{
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "onTextEnterCallback Called!");
}

static void onTextDeleteFallback(void *data)
{
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "onTextDeleteCallback Called!");
}

static void onSelectionChangeFallback(void *data)
{
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "onSelectionChangeCallback Called!");
}

/* Main GUI functions called by SGE.c */

bool SGE_GUI_Init()
{
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Initializing SGE GUI...");
	
	engine = SGE_GetEngineData();
	strcpy(panelsListStr, "Panel List");
	controlBoundsColor = SGE_COLOR_CERISE;
	
	/* Open GUI Fonts */
	buttonFont = TTF_OpenFont("assets/FreeSansBold.ttf", 18);
	if(buttonFont == NULL)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_ERROR, "Failed to load button font!");
		SGE_GUI_LogPrintLine(SGE_LOG_ERROR, "TTF_Error: %s", TTF_GetError());
		return false;
	}
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Opened Button font.");
	
	panelTitleFont = TTF_OpenFont("assets/FreeSansBold.ttf", 18);
	if(panelTitleFont == NULL)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_ERROR, "Failed to load panel title font!");
		SGE_GUI_LogPrintLine(SGE_LOG_ERROR, "TTF_Error: %s", TTF_GetError());
		return false;
	}
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Opened Panel Title font.");
	
	labelFont = TTF_OpenFont("assets/FreeSans.ttf", 18);
	if(labelFont == NULL)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_ERROR, "Failed to load default label font!");
		SGE_GUI_LogPrintLine(SGE_LOG_ERROR, "TTF_Error: %s", TTF_GetError());
		return false;
	}
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Opened default label font.");
	
	textBoxFont = TTF_OpenFont("assets/FreeSans.ttf", 18);
	if(textBoxFont == NULL)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_ERROR, "Failed to load default textInputBox font!");
		SGE_GUI_LogPrintLine(SGE_LOG_ERROR, "TTF_Error: %s", TTF_GetError());
		return false;
	}
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Opened default textInputBox font.");
	
	listBoxFont = TTF_OpenFont("assets/FreeSans.ttf", 18);
	if(listBoxFont == NULL)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_ERROR, "Failed to load default listBoxFont font!");
		SGE_GUI_LogPrintLine(SGE_LOG_ERROR, "TTF_Error: %s", TTF_GetError());
		return false;
	}
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Opened default listBoxFont font.");

	SGE_GUI_DebugState_Init();

	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Finished Initializing SGE GUI.");
	SGE_printf(SGE_LOG_DEBUG, "\n");
	return true;
}

void SGE_GUI_Quit()
{
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Quitting SGE GUI...");
	
	/* Close all GUI Fonts */
	if(buttonFont != NULL)
	{
		TTF_CloseFont(buttonFont);
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Closed Button font.");
	}
	
	if(panelTitleFont != NULL)
	{
		TTF_CloseFont(panelTitleFont);
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Closed Panel title font.");
	}
	
	if(labelFont != NULL)
	{
		TTF_CloseFont(labelFont);
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Closed default label font.");
	}
	
	if(textBoxFont != NULL)
	{
		TTF_CloseFont(textBoxFont);
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Closed default textBoxFont font.");
	}
	
	if(listBoxFont != NULL)
	{
		TTF_CloseFont(listBoxFont);
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Closed default listBoxFont font.");
	}

	SGE_GUI_FreeControlList(&debugStateControls);
	
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Finished Quitting SGE GUI.");
	SGE_printf(SGE_LOG_DEBUG, "\n");
}

void SGE_GUI_HandleEvents()
{
	if(engine->event.type == SDL_KEYDOWN)
	{
		/* Toggle debug state visibility */
		if(engine->event.key.keysym.sym == SDLK_F1)
		{
			SGE_SetTextureWordWrap(500);
			SGE_TextLabelSetText(panelListLabel, panelsListStr);
			SGE_TextLabelSetText(stateListLabel, SGE_GetStateNames());
			showDebugState = !showDebugState;
		}
		
		/* Toggle debug panel visibility */
		if(showDebugState)
		{
			if(engine->event.key.keysym.sym == SDLK_F2)
			{			
				debugPanel->isVisible = !debugPanel->isVisible;
			}
		}
	}
	
	SGE_GUI_ControlList_HandleEvents(currentStateControls);

	if(showDebugState)
	{
		SGE_GUI_ControlList *tempCurrentStateControls = currentStateControls;
		currentStateControls = &debugStateControls;
		SGE_GUI_ControlList_HandleEvents(&debugStateControls);
		currentStateControls = tempCurrentStateControls;
	}
}

void SGE_GUI_Update()
{
	SGE_GUI_ControlList_Update(currentStateControls);
	
	if(showDebugState)
	{
		SGE_GUI_ControlList *tempCurrentStateControls = currentStateControls;
		currentStateControls = &debugStateControls;
		SGE_GUI_DebugState_Update();
		SGE_GUI_ControlList_Update(&debugStateControls);
		currentStateControls = tempCurrentStateControls;
	}
}

void SGE_GUI_Render()
{
	SGE_GUI_ControlList_Render(currentStateControls);

	if(showDebugState)
	{
		SGE_GUI_ControlList *tempCurrentStateControls = currentStateControls;
		currentStateControls = &debugStateControls;
		SGE_GUI_ControlList_Render(&debugStateControls);
		currentStateControls = tempCurrentStateControls;
	}
}

/* Syncs GUI with game state */
void SGE_GUI_UpdateCurrentState(const char *nextState)
{
	int i;
	currentStateControls = SGE_GetStateGUIList(nextState);
	printPanelsStr();
	
	/* Reset control states */
	for(i = 0; i < currentStateControls->buttonCount; i++)
	{
		currentStateControls->buttons[i]->state = SGE_CONTROL_STATE_NORMAL;
	}

	for(i = 0; i < currentStateControls->checkBoxCount; i++)
	{
		currentStateControls->checkBoxes[i]->state = SGE_CONTROL_STATE_NORMAL;
	}

	for(i = 0; i < currentStateControls->sliderCount; i++)
	{
		currentStateControls->sliders[i]->state = SGE_CONTROL_STATE_NORMAL;
	}
}

static void SGE_GUI_ControlList_HandleEvents(SGE_GUI_ControlList *controls)
{
	int i = 0;

	SGE_WindowPanel **panels = controls->panels;
	int panelCount = controls->panelCount;
	
	/* Handle Events for parentless controls */
	for(i = 0; i < controls->buttonCount; i++)
	{
		SGE_ButtonHandleEvents(controls->buttons[i]);
	}
	
	for(i = 0; i < controls->checkBoxCount; i++)
	{
		SGE_CheckBoxHandleEvents(controls->checkBoxes[i]);
	}
	
	for(i = 0; i < controls->sliderCount; i++)
	{
		SGE_SliderHandleEvents(controls->sliders[i]);
	}
	
	for(i = 0; i < controls->textInputBoxCount; i++)
	{
		SGE_TextInputBoxHandleEvents(controls->textInputBoxes[i]);
	}
	
	for(i = 0; i < controls->listBoxCount; i++)
	{
		SGE_ListBoxHandleEvents(controls->listBoxes[i]);
	}
	
	/* Handle Events for all the panels themselves */
	for(i = 0; i < panelCount; i++)
	{
		if(panels[i]->isVisible)
			SGE_WindowPanelHandleEvents(panels[i]);
	}
	
	/* Handle Events for only the active panel's controls */
	if(panelCount != 0)
	{
		if(panels[panelCount - 1]->isVisible)
		{
			for(i = 0; i < panels[panelCount - 1]->buttonCount; i++)
			{
				SGE_ButtonHandleEvents(panels[panelCount - 1]->buttons[i]);
			}
			
			for(i = 0; i < panels[panelCount - 1]->checkBoxCount; i++)
			{
				SGE_CheckBoxHandleEvents(panels[panelCount - 1]->checkBoxes[i]);
			}
			
			for(i = 0; i < panels[panelCount - 1]->sliderCount; i++)
			{
				SGE_SliderHandleEvents(panels[panelCount - 1]->sliders[i]);
			}
			
			for(i = 0; i < panels[panelCount - 1]->textInputBoxCount; i++)
			{
				SGE_TextInputBoxHandleEvents(panels[panelCount - 1]->textInputBoxes[i]);
			}
			
			for(i = 0; i < panels[panelCount - 1]->listBoxCount; i++)
			{
				SGE_ListBoxHandleEvents(panels[panelCount - 1]->listBoxes[i]);
			}
		}
	}
}

static void SGE_GUI_ControlList_Update(SGE_GUI_ControlList *controls)
{
	int i = 0;
	int j = 0;

	SGE_WindowPanel **panels = controls->panels;
	int panelCount = controls->panelCount;

	panels = controls->panels;
	panelCount = controls->panelCount;
	
	/* Update the parentless controls */
	for(i = 0; i < controls->buttonCount; i++)
	{
		SGE_ButtonUpdate(controls->buttons[i]);
	}
	
	for(i = 0; i < controls->checkBoxCount; i++)
	{
		SGE_CheckBoxUpdate(controls->checkBoxes[i]);
	}
	
	for(i = 0; i < controls->sliderCount; i++)
	{
		SGE_SliderUpdate(controls->sliders[i]);
	}
	
	for(i = 0; i < controls->textInputBoxCount; i++)
	{
		SGE_TextInputBoxUpdate(controls->textInputBoxes[i]);
	}
	
	for(i = 0; i < controls->listBoxCount; i++)
	{
		SGE_ListBoxUpdate(controls->listBoxes[i]);
	}
	
	if(panelCount != 0)
	{
		for(i = 0; i < panelCount; i++)
		{
			if(panels[i]->isVisible)
			{
				SGE_WindowPanelUpdate(panels[i]);
				
				/* Update all panel's controls */
				for(j = 0; j < panels[i]->buttonCount; j++)
				{
					SGE_ButtonUpdate(panels[i]->buttons[j]);
				}
				
				for(j = 0; j < panels[i]->checkBoxCount; j++)
				{
					SGE_CheckBoxUpdate(panels[i]->checkBoxes[j]);
				}
				
				for(j = 0; j < panels[i]->sliderCount; j++)
				{
					SGE_SliderUpdate(panels[i]->sliders[j]);
				}
				
				for(j = 0; j < panels[i]->textInputBoxCount; j++)
				{
					SGE_TextInputBoxUpdate(panels[i]->textInputBoxes[j]);
				}
				
				for(j = 0; j < panels[i]->listBoxCount; j++)
				{
					SGE_ListBoxUpdate(panels[i]->listBoxes[j]);
				}
			}
		}
	}
}

static void SGE_GUI_ControlList_Render(SGE_GUI_ControlList *controls)
{
	int i = 0;
	
	/* Render all the panels and their child controls */
	for(i = 0; i < controls->panelCount; i++)
	{
		if(controls->panels[i]->isVisible)
		{
			SGE_WindowPanelRender(controls->panels[i]);
		}
	}
	
	/* Render the parentless controls */
	for(i = 0; i < controls->buttonCount; i++)
	{
		SGE_ButtonRender(controls->buttons[i]);
	}
	
	for(i = 0; i < controls->checkBoxCount; i++)
	{
		SGE_CheckBoxRender(controls->checkBoxes[i]);
	}
	
	for(i = 0; i < controls->labelCount; i++)
	{
		SGE_TextLabelRender(controls->labels[i]);
	}
	
	for(i = 0; i < controls->sliderCount; i++)
	{
		SGE_SliderRender(controls->sliders[i]);
	}
	
	for(i = 0; i < controls->textInputBoxCount; i++)
	{
		SGE_TextInputBoxRender(controls->textInputBoxes[i]);
	}
	
	for(i = 0; i < controls->listBoxCount; i++)
	{
		SGE_ListBoxRender(controls->listBoxes[i]);
	}
}

static void SGE_GUI_FreeControlList(SGE_GUI_ControlList *controls)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int l = 0;
	int m = 0;
	int n = 0; // Some of these are unnecessary \/(-_-)\/

	for(i = 0; i < controls->buttonCount; i++)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Destroyed Control: {NULL}-><Button> %d", i + 1);
		SGE_DestroyButton(controls->buttons[i]);
		controls->buttons[i] = NULL;
	}
	controls->buttonCount = 0;

	for(i = 0; i < controls->checkBoxCount; i++)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Destroyed Control: {NULL}-><Checkbox> %d", i + 1);
		SGE_DestroyCheckBox(controls->checkBoxes[i]);
		controls->checkBoxes[i] = NULL;
	}
	controls->checkBoxCount = 0;

	for(i = 0; i < controls->labelCount; i++)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Destroyed Control: {NULL}-><TextLabel> %d", i + 1);
		SGE_DestroyTextLabel(controls->labels[i]);
		controls->labels[i] = NULL;
	}
	controls->labelCount = 0;

	for(i = 0; i < controls->sliderCount; i++)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Destroyed Control: {NULL}-><Slider> %d", i + 1);
		SGE_DestroySlider(controls->sliders[i]);
		controls->sliders[i] = NULL;
	}
	controls->sliderCount = 0;

	for(i = 0; i < controls->textInputBoxCount; i++)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Destroyed Control: {NULL}-><TextInputBox> %d", i + 1);
		SGE_DestroyTextInputBox(controls->textInputBoxes[i]);
		controls->textInputBoxes[i] = NULL;
	}
	controls->textInputBoxCount = 0;

	for(i = 0; i < controls->listBoxCount; i++)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Destroyed Control: {NULL}-><ListBox> %d", i + 1);
		SGE_DestroyListBox(controls->listBoxes[i]);
		controls->listBoxes[i] = NULL;
	}
	controls->listBoxCount = 0;

	/* Go through each panel in the panels stack and destroy it's child controls and then the panel itself */
	for(i = 0; i < controls->panelCount; i++)
	{
		for(j = 0; j < controls->panels[i]->buttonCount; j++)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Destroyed Control: %s->Button %d", controls->panels[i]->titleStr, j + 1);
			SGE_DestroyButton(controls->panels[i]->buttons[j]);
		}
		for(k = 0; k < controls->panels[i]->checkBoxCount; k++)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Destroyed Control: %s->CheckBox %d", controls->panels[i]->titleStr, k + 1);
			SGE_DestroyCheckBox(controls->panels[i]->checkBoxes[k]);
		}
		for(l = 0; l < controls->panels[i]->textLabelCount; l++)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Destroyed Control: %s->Label %d", controls->panels[i]->titleStr, l + 1);
			SGE_DestroyTextLabel(controls->panels[i]->textLabels[l]);
		}
		for(m = 0; m < controls->panels[i]->sliderCount; m++)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Destroyed Control: %s->Slider %d", controls->panels[i]->titleStr, m + 1);
			SGE_DestroySlider(controls->panels[i]->sliders[m]);
		}
		for(n = 0; n < controls->panels[i]->textInputBoxCount; n++)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Destroyed Control: %s->TextInputBox %d", controls->panels[i]->titleStr, n + 1);
			SGE_DestroyTextInputBox(controls->panels[i]->textInputBoxes[n]);
		}
		for(n = 0; n < controls->panels[i]->listBoxCount; n++)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Destroyed Control: %s->ListBox %d", controls->panels[i]->titleStr, n + 1);
			SGE_DestroyListBox(controls->panels[i]->listBoxes[n]);
		}
		
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Destroyed Panel: %s", controls->panels[i]->titleStr);
		SGE_DestroyWindowPanel(controls->panels[i]);
		controls->panels[i] = NULL;
	}
	controls->panelCount = 0;
}

void SGE_GUI_FreeState(const char *name)
{
	SGE_GUI_ControlList *controls = SGE_GetStateGUIList(name);
	if(controls != NULL)
		SGE_GUI_FreeControlList(controls);
}

/* GUI Control Functions */

SGE_Button *SGE_CreateButton(const char *text, int x, int y, struct SGE_WindowPanel *panel)
{
	SGE_Button *button = NULL;
	button = (SGE_Button*)malloc(sizeof(SGE_Button));
	
	if(panel != NULL)
	{
		if(panel->buttonCount == PANEL_MAX_BUTTONS)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_WARNING, "Failed to add Button! Max amount of buttons [%d] in panel %s reached.", PANEL_MAX_BUTTONS, panel->titleStr);
			free(button);
			return NULL;
		}
		/* Add this new button to the top of the parent panel's buttons list */
		panel->buttons[panel->buttonCount] = button;
		panel->buttonCount += 1;
		panel->controlCount += 1;
		button->parentPanel = panel;
		button->alpha = button->parentPanel->alpha;
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Added Control: %s->Button %d", panel->titleStr, panel->buttonCount);
	}
	else
	{
		if(currentStateControls->buttonCount == STATE_MAX_BUTTONS)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_WARNING, "Failed to add Button! Max amount of parentless buttons [%d] reached.", STATE_MAX_BUTTONS);
			free(button);
			return NULL;
		}
		/* Add this new button to the top of the parentless buttons list */
		currentStateControls->buttons[currentStateControls->buttonCount] = button;
		currentStateControls->buttonCount += 1;
		button->parentPanel = NULL;
		button->alpha = 255;
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Added Control: {NULL}->Button %d", currentStateControls->buttonCount);
	}
	
	button->x = x;
	button->y = y;
	
	button->state = SGE_CONTROL_STATE_NORMAL;
	
	button->normalColor = SGE_COLOR_DARK_RED;
	button->hoverColor = SGE_COLOR_GRAY;
	button->clickedColor = SGE_COLOR_LIGHT_GRAY;
	button->currentColor = button->normalColor;
	
	button->textImg = SGE_CreateTextureFromText(text, buttonFont, SGE_COLOR_WHITE, SGE_TEXT_MODE_BLENDED);
	if(buttonFont == NULL)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_ERROR, "Failed to create button text image!");
		TTF_CloseFont(buttonFont);
		free(button);
		return NULL;
	}
	
	/* Calculate the bounding box based on parent panel position */
	if(button->parentPanel != NULL)
	{
		button->boundBox.x = button->x + button->parentPanel->background.x + button->parentPanel->x_scroll_offset;
		button->boundBox.y = button->y + button->parentPanel->background.y + button->parentPanel->y_scroll_offset;
	}
	else
	{
		button->boundBox.x = button->x;
		button->boundBox.y = button->y;
	}
	
	/* Calculate the position and size of the button */
	button->background.x = button->boundBox.x;
	button->background.y = button->boundBox.y;
	button->background.w = button->textImg->w + 20;
	button->background.h = button->textImg->h + 20;
	button->textImg->x = button->background.x + (button->background.w / 2) - button->textImg->w / 2;
	button->textImg->y = button->background.y + (button->background.h / 2) - button->textImg->h / 2;
	
	button->boundBox.w = button->background.w;
	button->boundBox.h = button->background.h;
	
	/* Set button callback functions to fallback */
	button->onMouseDown = onDownFallbackCallback;
	button->onMouseDown_data = NULL;
	button->onMouseUp = onUpFallbackCallback;
	button->onMouseUp_data = NULL;
	
	/* Recalculate the parent panel's MCR */
	if(panel != NULL)
	{
		SGE_WindowPanelCalculateMCR(panel, button->boundBox);
	}
	
	return button;
}

void SGE_DestroyButton(SGE_Button *button)
{
	if(button != NULL)
	{
		SGE_FreeTexture(button->textImg);
		free(button);
	}
}

void SGE_ButtonHandleEvents(SGE_Button *button)
{
	if(engine->event.type == SDL_MOUSEBUTTONDOWN)
	{
		if(engine->event.button.button == 1)
		{
			if(SGE_isMouseOver(&button->boundBox))
			{
				if(button->parentPanel != NULL)
				{
					if(SGE_isMouseOver(&button->parentPanel->background) && !SGE_isMouseOver(&button->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&button->parentPanel->verticalScrollbarBG))
					{
						button->state = SGE_CONTROL_STATE_CLICKED;
						button->onMouseDown(button->onMouseDown_data);
					}
				}
				else
				{
					button->state = SGE_CONTROL_STATE_CLICKED;
					button->onMouseDown(button->onMouseDown_data);
				}
			}
		}
	}
	else if(engine->event.type == SDL_MOUSEBUTTONUP)
	{
		if(engine->event.button.button == 1)
		{
			if(button->state == SGE_CONTROL_STATE_CLICKED)
			{
				if(SGE_isMouseOver(&button->boundBox))
				{
					if(button->parentPanel != NULL)
					{
						if(SGE_isMouseOver(&button->parentPanel->background) && !SGE_isMouseOver(&button->parentPanel->horizontalScrollbarBG)&& !SGE_isMouseOver(&button->parentPanel->verticalScrollbarBG))
						{
							button->state = SGE_CONTROL_STATE_HOVER;
							button->onMouseUp(button->onMouseUp_data);
						}
					}
					else
					{
						button->state = SGE_CONTROL_STATE_HOVER;
						button->onMouseUp(button->onMouseUp_data);
					}
				}
				else
				{
					button->state = SGE_CONTROL_STATE_NORMAL;
				}
			}
		}
	}
	else if(engine->event.type == SDL_MOUSEMOTION)
	{
		if(button->state != SGE_CONTROL_STATE_CLICKED)
		{
			if(SGE_isMouseOver(&button->boundBox))
			{
				if(button->parentPanel != NULL)
				{
					if(SGE_isMouseOver(&button->parentPanel->background) && !SGE_isMouseOver(&button->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&button->parentPanel->verticalScrollbarBG))
						button->state = SGE_CONTROL_STATE_HOVER;
					else
						button->state = SGE_CONTROL_STATE_NORMAL;
				}
				else
					button->state = SGE_CONTROL_STATE_HOVER;
			}
			else
				button->state = SGE_CONTROL_STATE_NORMAL;
		}
	}
}

void SGE_ButtonUpdate(SGE_Button *button)
{
	if(button->state == SGE_CONTROL_STATE_NORMAL)
		button->currentColor = button->normalColor;
	else if(button->state == SGE_CONTROL_STATE_HOVER)
		button->currentColor = button->hoverColor;
	else if(button->state == SGE_CONTROL_STATE_CLICKED)
		button->currentColor = button->clickedColor;
	
	if(button->parentPanel != NULL)
	{
		button->boundBox.x = button->x + button->parentPanel->background.x + button->parentPanel->x_scroll_offset;
		button->boundBox.y = button->y + button->parentPanel->background.y + button->parentPanel->y_scroll_offset;
		button->background.x = button->boundBox.x;
		button->background.y = button->boundBox.y;
		button->textImg->x = button->background.x + (button->background.w / 2) - button->textImg->w / 2;
		button->textImg->y = button->background.y + (button->background.h / 2) - button->textImg->h / 2;
		button->alpha = button->parentPanel->alpha;
	}
}

void SGE_ButtonRender(SGE_Button *button)
{
	int i = 0;
	
	/* Draw filled button background */
	SDL_SetRenderDrawColor(engine->renderer, button->currentColor.r, button->currentColor.g, button->currentColor.b, button->alpha);
	SDL_RenderFillRect(engine->renderer, &button->background);
	
	/* Draw button border */
	SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, button->alpha);
	if(SGE_isMouseOver(&button->boundBox))
	{
		if(button->parentPanel != NULL)
		{
			if(SGE_isMouseOver(&button->parentPanel->background) && !SGE_isMouseOver(&button->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&button->parentPanel->verticalScrollbarBG))
				SDL_SetRenderDrawColor(engine->renderer, 225, 225, 225, button->alpha);
			
			for(i = button->parentPanel->index + 1; i < currentStateControls->panelCount; i++)
			{
				if(SGE_isMouseOver(&currentStateControls->panels[i]->border))
					SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, button->alpha);
			}
		}
		else
			SDL_SetRenderDrawColor(engine->renderer, 225, 225, 225, button->alpha);
	}
	SDL_RenderDrawRect(engine->renderer, &button->background);
	
	/* Draw button text image */
	SGE_SetTextureAlpha(button->textImg, button->alpha);
	SGE_RenderTexture(button->textImg);
	
	if(showControlBounds)
	{
		SDL_SetRenderDrawColor(engine->renderer, controlBoundsColor.r, controlBoundsColor.g, controlBoundsColor.b, button->alpha);
		SDL_RenderDrawRect(engine->renderer, &button->boundBox);
	}
}

void SGE_ButtonSetPosition(SGE_Button *button, int x, int y)
{
	button->x = x;
	button->y = y;
	
	/* Calculate the bounding box based on parent panel position */
	if(button->parentPanel != NULL)
	{
		button->boundBox.x = button->x + button->parentPanel->background.x + button->parentPanel->x_scroll_offset;
		button->boundBox.y = button->y + button->parentPanel->background.y + button->parentPanel->y_scroll_offset;
	}
	else
	{
		button->boundBox.x = button->x;
		button->boundBox.y = button->y;
	}
	
	button->background.x = button->boundBox.x;
	button->background.y = button->boundBox.y;
	button->textImg->x = button->background.x + (button->background.w / 2) - button->textImg->w / 2;
	button->textImg->y = button->background.y + (button->background.h / 2) - button->textImg->h / 2;
	
	/* Recalculate the parent panel's MCR */
	if(button->parentPanel != NULL)
	{
		SGE_WindowPanelCalculateMCR(button->parentPanel, button->boundBox);
	}
}

SGE_CheckBox *SGE_CreateCheckBox(int x, int y, struct SGE_WindowPanel *panel)
{
	SGE_CheckBox *checkBox = NULL;
	checkBox = (SGE_CheckBox *)malloc(sizeof(SGE_CheckBox));
	
	if(panel != NULL)
	{
		if(panel->checkBoxCount == PANEL_MAX_CHECKBOXES)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_WARNING, "Failed to add CheckBox! Max amount of checkboxes [%d] in panel %s reached.", PANEL_MAX_CHECKBOXES, panel->titleStr);
			free(checkBox);
			return NULL;
		}
		/* Add this new checkbox to the top of the parent panel's checkboxes list */
		panel->checkBoxes[panel->checkBoxCount] = checkBox;
		panel->checkBoxCount += 1;
		panel->controlCount += 1;
		checkBox->parentPanel = panel;
		checkBox->alpha = checkBox->parentPanel->alpha;
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Added Control: %s->CheckBox %d", panel->titleStr, panel->checkBoxCount);
	}
	else
	{
		if(currentStateControls->checkBoxCount == STATE_MAX_CHECKBOXES)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_WARNING, "Failed to add CheckBox! Max amount of parentless checkboxes [%d] reached.", STATE_MAX_CHECKBOXES);
			free(checkBox);
			return NULL;
		}
		/* Add this new checkbox to the top of the parentless checkboxes list */
		currentStateControls->checkBoxes[currentStateControls->checkBoxCount] = checkBox;
		currentStateControls->checkBoxCount += 1;
		checkBox->parentPanel = NULL;
		checkBox->alpha = 255;
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Added Control: {NULL}->CheckBox %d", currentStateControls->checkBoxCount);
	}
	
	checkBox->x = x;
	checkBox->y = y;
	checkBox->size = 30;
	
	if(checkBox->parentPanel != NULL)
	{
		checkBox->boundBox.x = checkBox->x + checkBox->parentPanel->background.x + checkBox->parentPanel->x_scroll_offset;
		checkBox->boundBox.y = checkBox->y + checkBox->parentPanel->background.y + checkBox->parentPanel->y_scroll_offset;
	}
	else
	{
		checkBox->boundBox.x = checkBox->x;
		checkBox->boundBox.y = checkBox->y;
	}
	
	/* Calculate checkbox location and size */
	checkBox->bg.x = checkBox->boundBox.x;
	checkBox->bg.y = checkBox->boundBox.y;
	checkBox->bg.w = checkBox->size;
	checkBox->bg.h = checkBox->size;
	
	checkBox->boundBox.w = checkBox->bg.w;
	checkBox->boundBox.h = checkBox->bg.h;
	
	checkBox->check.w = checkBox->size - 10;
	checkBox->check.h = checkBox->size - 10;
	checkBox->check.x = checkBox->bg.x + (checkBox->bg.w / 2) - (checkBox->check.w / 2);
	checkBox->check.y = checkBox->bg.y + (checkBox->bg.h / 2) - (checkBox->check.h / 2);
	
	checkBox->checkColor = SGE_COLOR_DARK_RED;
	checkBox->isChecked = false;
	
	checkBox->state = SGE_CONTROL_STATE_NORMAL;
	checkBox->onMouseDown = onDownFallbackCallback;
	checkBox->onMouseDown_data = NULL;
	checkBox->onMouseUp = onUpFallbackCallback;
	checkBox->onMouseUp_data = NULL;
	
	/* Recalculate the parent panel's MCR */
	if(panel != NULL)
	{
		SGE_WindowPanelCalculateMCR(panel, checkBox->boundBox);
	}
	
	return checkBox;
}

void SGE_DestroyCheckBox(SGE_CheckBox *checkBox)
{
	if(checkBox != NULL)
	{
		free(checkBox);
	}
}

void SGE_CheckBoxHandleEvents(SGE_CheckBox *checkBox)
{
	if(engine->event.type == SDL_MOUSEBUTTONDOWN)
	{
		if(engine->event.button.button == 1)
		{
			if(SGE_isMouseOver(&checkBox->boundBox))
			{
				if(checkBox->parentPanel != NULL)
				{
					if(SGE_isMouseOver(&checkBox->parentPanel->background) && !SGE_isMouseOver(&checkBox->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&checkBox->parentPanel->verticalScrollbarBG))
					{
						checkBox->state = SGE_CONTROL_STATE_CLICKED;
						checkBox->onMouseDown(checkBox->onMouseDown_data);
					}
				}
				else
				{
					checkBox->state = SGE_CONTROL_STATE_CLICKED;
					checkBox->onMouseDown(checkBox->onMouseDown_data);
				}
			}
		}
	}
	else if(engine->event.type == SDL_MOUSEBUTTONUP)
	{
		if(engine->event.button.button == 1)
		{
			if(checkBox->state == SGE_CONTROL_STATE_CLICKED)
			{
				if(SGE_isMouseOver(&checkBox->boundBox))
				{
					checkBox->isChecked = !checkBox->isChecked;
					checkBox->state = SGE_CONTROL_STATE_HOVER;
					checkBox->onMouseUp(checkBox->onMouseUp_data);
				}
				else
				{
					checkBox->state = SGE_CONTROL_STATE_NORMAL;
				}
			}
		}
	}
	else if(engine->event.type == SDL_MOUSEMOTION)
	{
		if(checkBox->state != SGE_CONTROL_STATE_CLICKED)
		{
			if(SGE_isMouseOver(&checkBox->boundBox))
			{
				if(checkBox->parentPanel != NULL)
				{
					if(SGE_isMouseOver(&checkBox->parentPanel->background) && !SGE_isMouseOver(&checkBox->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&checkBox->parentPanel->verticalScrollbarBG))
						checkBox->state = SGE_CONTROL_STATE_HOVER;
					else
						checkBox->state = SGE_CONTROL_STATE_NORMAL;
				}
				else
					checkBox->state = SGE_CONTROL_STATE_HOVER;
			}
			else
				checkBox->state = SGE_CONTROL_STATE_NORMAL;
		}
	}
}

void SGE_CheckBoxUpdate(SGE_CheckBox *checkBox)
{
	/* Recalculate checkbox location based on the parent panel */
	if(checkBox->parentPanel != NULL)
	{
		checkBox->boundBox.x = checkBox->x + checkBox->parentPanel->background.x + checkBox->parentPanel->x_scroll_offset;
		checkBox->boundBox.y = checkBox->y + checkBox->parentPanel->background.y + checkBox->parentPanel->y_scroll_offset;
		checkBox->bg.x = checkBox->boundBox.x;
		checkBox->bg.y = checkBox->boundBox.y;
		checkBox->check.x = checkBox->bg.x + (checkBox->bg.w / 2) - (checkBox->check.w / 2);
		checkBox->check.y = checkBox->bg.y + (checkBox->bg.h / 2) - (checkBox->check.h / 2);
		checkBox->alpha = checkBox->parentPanel->alpha;
	}
}

void SGE_CheckBoxRender(SGE_CheckBox *checkBox)
{
	int i = 0;
	
	/* Draw white checkbox filled background */
	SDL_SetRenderDrawColor(engine->renderer, 255, 255, 255, checkBox->alpha);
	SDL_RenderFillRect(engine->renderer, &checkBox->bg);
	
	/* Draw gray checkbox border */
	SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, checkBox->alpha);
	if(SGE_isMouseOver(&checkBox->boundBox))
	{
		if(checkBox->parentPanel != NULL)
		{
			if(SGE_isMouseOver(&checkBox->parentPanel->background) && !SGE_isMouseOver(&checkBox->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&checkBox->parentPanel->verticalScrollbarBG))
				SDL_SetRenderDrawColor(engine->renderer, 150, 150, 150, checkBox->alpha);
			
			for(i = checkBox->parentPanel->index + 1; i < currentStateControls->panelCount; i++)
			{
				if(SGE_isMouseOver(&currentStateControls->panels[i]->border))
					SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, checkBox->alpha);
			}
		}
		else
			SDL_SetRenderDrawColor(engine->renderer, 150, 150, 150, checkBox->alpha);
	}
	SDL_RenderDrawRect(engine->renderer, &checkBox->bg);
	
	/* Draw the check inside the background */
	if(checkBox->isChecked == true)
	{
		SDL_SetRenderDrawColor(engine->renderer, checkBox->checkColor.r, checkBox->checkColor.g, checkBox->checkColor.b, checkBox->alpha);
		SDL_RenderFillRect(engine->renderer, &checkBox->check);
	}
	
	if(showControlBounds)
	{
		SDL_SetRenderDrawColor(engine->renderer, controlBoundsColor.r, controlBoundsColor.g, controlBoundsColor.b, checkBox->alpha);
		SDL_RenderDrawRect(engine->renderer, &checkBox->boundBox);
	}
}

void SGE_CheckBoxSetPosition(SGE_CheckBox *checkBox, int x, int y)
{
	checkBox->x = x;
	checkBox->y = y;
	
	/* Calculate the bounding box based on parent panel position */
	if(checkBox->parentPanel != NULL)
	{
		checkBox->boundBox.x = checkBox->x + checkBox->parentPanel->background.x + checkBox->parentPanel->x_scroll_offset;
		checkBox->boundBox.y = checkBox->y + checkBox->parentPanel->background.y + checkBox->parentPanel->y_scroll_offset;
	}
	else
	{
		checkBox->boundBox.x = checkBox->x;
		checkBox->boundBox.y = checkBox->y;
	}
	
	checkBox->bg.x = checkBox->boundBox.x;
	checkBox->bg.y = checkBox->boundBox.y;
	checkBox->check.x = checkBox->bg.x + (checkBox->bg.w / 2) - (checkBox->check.w / 2);
	checkBox->check.y = checkBox->bg.y + (checkBox->bg.h / 2) - (checkBox->check.h / 2);
	
	/* Recalculate the parent panel's MCR */
	if(checkBox->parentPanel != NULL)
	{
		SGE_WindowPanelCalculateMCR(checkBox->parentPanel, checkBox->boundBox);
	}
}

SGE_TextLabel *SGE_CreateTextLabelCustom(const char *text, int x, int y, SDL_Color color, TTF_Font *font, struct SGE_WindowPanel *panel)
{
	SGE_TextLabel *label = NULL;
	label = (SGE_TextLabel *)malloc(sizeof(SGE_TextLabel));
	
	if(panel != NULL)
	{
		if(panel->textLabelCount == PANEL_MAX_LABELS)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_WARNING, "Failed to add Label! Max amount of labels [%d] in panel %s reached.", PANEL_MAX_LABELS, panel->titleStr);
			free(label);
			return NULL;
		}
		panel->textLabels[panel->textLabelCount] = label;
		panel->textLabelCount += 1;
		panel->controlCount += 1;
		label->parentPanel = panel;
		label->alpha = label->parentPanel->alpha;
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Added Control: %s->Label %d", panel->titleStr, panel->textLabelCount);
	}
	else
	{
		if(currentStateControls->labelCount == STATE_MAX_LABELS)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_WARNING, "Failed to add Label! Max amount of parentless labels [%d] reached.", STATE_MAX_LABELS);
			free(label);
			return NULL;
		}
		currentStateControls->labels[currentStateControls->labelCount] = label;
		currentStateControls->labelCount += 1;
		label->parentPanel = NULL;
		label->alpha = 255;
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Added Control: {NULL}->Label %d", currentStateControls->labelCount);
	}
	
	label->x = x;
	label->y = y;
	
	strncpy(label->text, text, 200);
	label->font = font;
	label->fgColor = color;
	label->bgColor = SGE_COLOR_GRAY;
	label->showBG = false;
	label->mode = SGE_TEXT_MODE_BLENDED;
	label->isVisible = true;
	//SGE_SetTextureWordWrap();
	label->textImg = SGE_CreateTextureFromText(text, font, label->fgColor, label->mode);
	
	if(label->parentPanel != NULL)
	{
		label->boundBox.x = label->x + label->parentPanel->background.x + label->parentPanel->x_scroll_offset;
		label->boundBox.y = label->y + label->parentPanel->background.y + label->parentPanel->y_scroll_offset;
	}
	else
	{
		label->boundBox.x = label->x;
		label->boundBox.y = label->y;
	}
	
	label->textImg->x = label->boundBox.x;
	label->textImg->y = label->boundBox.y;
	label->boundBox.w = label->textImg->w;
	label->boundBox.h = label->textImg->h;
	
	/* Recalculate the parent panel's MCR */
	if(panel != NULL)
	{
		SGE_WindowPanelCalculateMCR(panel, label->boundBox);
	}
	
	return label;
}

SGE_TextLabel *SGE_CreateTextLabel(const char *text, int x, int y, SDL_Color color, struct SGE_WindowPanel *panel)
{
	return SGE_CreateTextLabelCustom(text, x, y, color, labelFont, panel);
}

void SGE_DestroyTextLabel(SGE_TextLabel *label)
{
	if(label != NULL)
	{
		SGE_FreeTexture(label->textImg);
		free(label);
	}
}

void SGE_TextLabelRender(SGE_TextLabel *label)
{
	if(!label->isVisible)
	{
		return;
	}

	if(label->parentPanel != NULL)
	{
		label->boundBox.x = label->x + label->parentPanel->background.x + label->parentPanel->x_scroll_offset;
		label->boundBox.y = label->y + label->parentPanel->background.y + label->parentPanel->y_scroll_offset;
		label->textImg->x = label->boundBox.x;
		label->textImg->y = label->boundBox.y;
		label->alpha = label->parentPanel->alpha;
		label->bgColor.a = label->alpha;
		SGE_SetTextureAlpha(label->textImg, label->alpha);
	}
	
	if(label->showBG)
	{
		SDL_SetRenderDrawColor(engine->renderer, label->bgColor.r, label->bgColor.g, label->bgColor.b, label->bgColor.a);
		SDL_RenderFillRect(engine->renderer, &label->boundBox);
	}
	
	SGE_RenderTexture(label->textImg);
	
	if(showControlBounds)
	{
		SDL_SetRenderDrawColor(engine->renderer, controlBoundsColor.r, controlBoundsColor.g, controlBoundsColor.b, label->alpha);
		SDL_RenderDrawRect(engine->renderer, &label->boundBox);
	}
}

void SGE_TextLabelSetPosition(SGE_TextLabel *label, int x, int y)
{
	label->x = x;
	label->y = y;
	
	/* Calculate the bounding box based on parent panel position */
	if(label->parentPanel != NULL)
	{
		label->boundBox.x = label->x + label->parentPanel->background.x + label->parentPanel->x_scroll_offset;
		label->boundBox.y = label->y + label->parentPanel->background.y + label->parentPanel->y_scroll_offset;
	}
	else
	{
		label->boundBox.x = label->x;
		label->boundBox.y = label->y;
	}
	
	label->textImg->x = label->boundBox.x;
	label->textImg->y = label->boundBox.y;
	
	/* Recalculate the parent panel's MCR */
	if(label->parentPanel != NULL)
	{
		SGE_WindowPanelCalculateMCR(label->parentPanel, label->boundBox);
	}
}

void SGE_TextLabelSetText(SGE_TextLabel *label, const char *text)
{
	strncpy(label->text, text, 200);
	SGE_UpdateTextureFromText(label->textImg, label->text, label->font, label->fgColor, label->mode);
	label->boundBox.w = label->textImg->w;
	label->boundBox.h = label->textImg->h;
	
	if(label->parentPanel != NULL)
	{
		SGE_WindowPanelCalculateMCR(label->parentPanel, label->boundBox);
	}
}

void SGE_TextLabelSetTextf(SGE_TextLabel *label, const char *format, ...)
{
    va_list args;
	va_start(args, format);

    vsnprintf(label->text, 200, format, args);
	SGE_UpdateTextureFromText(label->textImg, label->text, label->font, label->fgColor, label->mode);
	label->boundBox.w = label->textImg->w;
	label->boundBox.h = label->textImg->h;
	
	if(label->parentPanel != NULL)
	{
		SGE_WindowPanelCalculateMCR(label->parentPanel, label->boundBox);
	}

	va_end(args);
}

void SGE_TextLabelSetFGColor(SGE_TextLabel *label, SDL_Color fg)
{
	label->fgColor = fg;
	SGE_UpdateTextureFromText(label->textImg, label->text, labelFont, label->fgColor, label->mode);
}

void SGE_TextLabelSetBGColor(SGE_TextLabel *label, SDL_Color bg)
{
	label->bgColor = bg;
	if(!label->showBG)
	{
		label->showBG = true;
	}
}

void SGE_TextLabelSetMode(SGE_TextLabel *label, SGE_TextRenderMode mode)
{
	label->mode = mode;
	SGE_UpdateTextureFromText(label->textImg, label->text, labelFont, label->fgColor, label->mode);
}

void SGE_TextLabelSetAlpha(SGE_TextLabel *label, Uint8 alpha)
{
	SGE_SetTextureAlpha(label->textImg, alpha);
}

void SGE_TextLabelSetVisible(SGE_TextLabel *label, bool visible)
{
	label->isVisible = visible;
}

SGE_Slider *SGE_CreateSlider(int x, int y, struct SGE_WindowPanel *panel)
{
	SGE_Slider *slider = NULL;
	slider = (SGE_Slider *)malloc(sizeof(SGE_Slider));
	
	slider->x = x;
	slider->y = y;
	
	if(panel != NULL)
	{
		if(panel->sliderCount == PANEL_MAX_SLIDERS)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_WARNING, "Failed to add Slider! Max amount of sliders [%d] in panel %s reached.", PANEL_MAX_SLIDERS, panel->titleStr);
			free(slider);
			return NULL;
		}
		panel->sliders[panel->sliderCount] = slider;
		panel->sliderCount += 1;
		panel->controlCount += 1;
		slider->parentPanel = panel;
		slider->alpha = slider->parentPanel->alpha;
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Added Control: %s->Slider %d", panel->titleStr, panel->sliderCount);
	}
	else
	{
		if(currentStateControls->sliderCount == STATE_MAX_SLIDERS)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_WARNING, "Failed to add Slider! Max amount of parentless sliders [%d] reached.", STATE_MAX_SLIDERS);
			free(slider);
			return NULL;
		}
		currentStateControls->sliders[currentStateControls->sliderCount] = slider;
		currentStateControls->sliderCount += 1;
		slider->parentPanel = NULL;
		slider->alpha = 255;
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Added Control: {NULL}->Slider %d", currentStateControls->sliderCount);
	}
	
	if(slider->parentPanel != NULL)
	{
		slider->boundBox.x = slider->x + slider->parentPanel->background.x + slider->parentPanel->x_scroll_offset;
		slider->boundBox.y = slider->y + slider->parentPanel->background.y + slider->parentPanel->y_scroll_offset;
	}
	else
	{
		slider->boundBox.x = slider->x;
		slider->boundBox.y = slider->y;
	}
	
	slider->bar.w = 125;
	slider->bar.h = 5;
	slider->slider.w = 12;
	slider->slider.h = 25;
	
	slider->bar.x = slider->boundBox.x;
	slider->bar.y = slider->boundBox.y + (slider->slider.h / 2) - (slider->bar.h / 2);
	
	slider->x_offset = (double)(slider->slider.w) / (double)slider->bar.w;
	slider->value = 0.5f;
	slider->value_i = slider->value / (1 / (1 - slider->x_offset));
	slider->slider_xi = slider->bar.x + (slider->value_i * slider->bar.w);
	slider->slider.x = slider->slider_xi;
	slider->slider.y = slider->bar.y - (slider->slider.h / 2) + (slider->bar.h / 2);
	
	slider->sliderColor = SGE_COLOR_DARK_RED;
	slider->barColor = SGE_COLOR_WHITE;
	slider->move_dx = 0;
	
	slider->boundBox.w = slider->bar.w;
	slider->boundBox.h = slider->slider.h;
	
	slider->state = SGE_CONTROL_STATE_NORMAL;
	slider->onMouseDown = onDownFallbackCallback;
	slider->onMouseDown_data = NULL;
	slider->onMouseUp = onUpFallbackCallback;
	slider->onMouseUp_data = NULL;
	slider->onSlide = onSlideFallback;
	slider->onSlide_data = NULL;
	
	/* Recalculate the parent panel's MCR */
	if(panel != NULL)
	{
		SGE_WindowPanelCalculateMCR(panel, slider->boundBox);
	}
	
	return slider;
}

void SGE_DestroySlider(SGE_Slider *slider)
{
	if(slider != NULL)
	{
		free(slider);
	}
}

void SGE_SliderHandleEvents(SGE_Slider *slider)
{
	if(engine->event.type == SDL_MOUSEBUTTONDOWN)
	{
		if(engine->event.button.button == 1)
		{
			if(SGE_isMouseOver(&slider->slider))
			{
				if(slider->parentPanel != NULL)
				{
					if(SGE_isMouseOver(&slider->parentPanel->background) && !SGE_isMouseOver(&slider->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&slider->parentPanel->verticalScrollbarBG))
					{
						slider->state = SGE_CONTROL_STATE_CLICKED;
						slider->move_dx = engine->mouse_x - slider->slider.x;
						slider->onMouseDown(slider->onMouseDown_data);
					}
				}
				else
				{
					slider->state = SGE_CONTROL_STATE_CLICKED;
					slider->move_dx = engine->mouse_x - slider->slider.x;
					slider->onMouseDown(slider->onMouseDown_data);
				}
			}
			else if(SGE_isMouseOver(&slider->bar))
			{
				if(slider->parentPanel != NULL)
				{
					if(SGE_isMouseOver(&slider->parentPanel->background) && !SGE_isMouseOver(&slider->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&slider->parentPanel->verticalScrollbarBG))
					{
						slider->state = SGE_CONTROL_STATE_CLICKED;
						slider->slider_xi = engine->mouse_x - (slider->slider.w / 2);
						slider->slider.x = slider->slider_xi;
						slider->move_dx = engine->mouse_x - slider->slider.x;
						SGE_SliderUpdateValue(slider);
						slider->onMouseDown(slider->onMouseDown_data);
						slider->onSlide(slider->onSlide_data);
					}
				}
				else
				{
					slider->state = SGE_CONTROL_STATE_CLICKED;
					slider->slider_xi = engine->mouse_x - (slider->slider.w / 2);
					slider->slider.x = slider->slider_xi;
					slider->move_dx = engine->mouse_x - slider->slider.x;
					SGE_SliderUpdateValue(slider);
					slider->onMouseDown(slider->onMouseDown_data);
					slider->onSlide(slider->onSlide_data);
				}
			}
		}
	}
	else if(engine->event.type == SDL_MOUSEBUTTONUP)
	{
		if(engine->event.button.button == 1)
		{
			if(slider->state == SGE_CONTROL_STATE_CLICKED)
			{
				slider->onMouseUp(slider->onMouseUp_data);
				if(SGE_isMouseOver(&slider->slider))
				{
					slider->state = SGE_CONTROL_STATE_HOVER;
				}
				else
				{
					slider->state = SGE_CONTROL_STATE_NORMAL;
				}
			}
		}
	}
	else if(engine->event.type == SDL_MOUSEMOTION)
	{
		if(slider->state == SGE_CONTROL_STATE_CLICKED)
		{
			slider->onSlide(slider->onSlide_data);
		}
	}
}

void SGE_SliderUpdate(SGE_Slider *slider)
{
	/* Move the slider to new position */
	if(slider->state == SGE_CONTROL_STATE_CLICKED)
	{
		/* This line causes the slider click bug that changes the value set using the
		 * SGE_SliderSetValue() function.
		 * Clicking the slider causes this line to recalculate the value of slider_xi set 
		 * by the function, this recalculation uses mouse_x and move_dx which are integers
		 * and so slider_xi loses it's decimal part.
		 * Possible fix is to skip recalculation if there is no difference in the result
		 * and slider_xi apart from the decimal part.
		*/
		if((engine->mouse_x - slider->move_dx) != (int)slider->slider_xi)
		{
			slider->slider_xi = engine->mouse_x - slider->move_dx;
			if(slider->slider_xi < slider->bar.x)
				slider->slider_xi = slider->bar.x;
			if(slider->slider_xi > slider->bar.x + slider->bar.w  - slider->slider.w)
				slider->slider_xi = slider->bar.x + slider->bar.w - slider->slider.w;
			
			slider->slider.x = slider->slider_xi;
			SGE_SliderUpdateValue(slider);
		}
	}
	
	/* Recalculate position based on panel */
	if(slider->parentPanel != NULL)
	{
		slider->boundBox.x = slider->x + slider->parentPanel->background.x + slider->parentPanel->x_scroll_offset;
		slider->boundBox.y = slider->y + slider->parentPanel->background.y + slider->parentPanel->y_scroll_offset;
		slider->bar.x = slider->boundBox.x;
		slider->bar.y = slider->boundBox.y + (slider->slider.h / 2) - (slider->bar.h / 2);
		slider->slider_xi = slider->bar.x + (slider->value_i * slider->bar.w);
		slider->slider.x = slider->slider_xi;
		slider->slider.y = slider->bar.y - (slider->slider.h / 2) + (slider->bar.h / 2);
		slider->alpha = slider->parentPanel->alpha;
	}
}

void SGE_SliderRender(SGE_Slider *slider)
{
	int i = 0;
	
	SDL_SetRenderDrawColor(engine->renderer, slider->barColor.r, slider->barColor.g, slider->barColor.b, slider->alpha);
	SDL_RenderFillRect(engine->renderer, &slider->bar);
	SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, slider->alpha);
	SDL_RenderDrawRect(engine->renderer, &slider->bar);
	
	SDL_SetRenderDrawColor(engine->renderer, slider->sliderColor.r, slider->sliderColor.g, slider->sliderColor.b, slider->alpha);
	SDL_RenderFillRect(engine->renderer, &slider->slider);
	
	SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, slider->alpha);
	if(SGE_isMouseOver(&slider->slider) || slider->state == SGE_CONTROL_STATE_CLICKED)
	{
		if(slider->parentPanel != NULL)
		{
			if(SGE_isMouseOver(&slider->parentPanel->background) && !SGE_isMouseOver(&slider->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&slider->parentPanel->verticalScrollbarBG))
				SDL_SetRenderDrawColor(engine->renderer, 225, 225, 225, slider->alpha);
			
			for(i = slider->parentPanel->index + 1; i < currentStateControls->panelCount; i++)
			{
				if(SGE_isMouseOver(&currentStateControls->panels[i]->border))
					SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, slider->alpha);
			}
		}
		else
			SDL_SetRenderDrawColor(engine->renderer, 225, 225, 225, slider->alpha);
	}
	SDL_RenderDrawRect(engine->renderer, &slider->slider);
	
	if(showControlBounds)
	{
		SDL_SetRenderDrawColor(engine->renderer, controlBoundsColor.r, controlBoundsColor.g, controlBoundsColor.b, slider->alpha);
		SDL_RenderDrawRect(engine->renderer, &slider->boundBox);
	}
}

void SGE_SliderSetPosition(SGE_Slider *slider, int x, int y)
{
	slider->x = x;
	slider->y = y;
	
	/* Calculate the bounding box based on parent panel position */
	if(slider->parentPanel != NULL)
	{
		slider->boundBox.x = slider->x + slider->parentPanel->background.x + slider->parentPanel->x_scroll_offset;
		slider->boundBox.y = slider->y + slider->parentPanel->background.y + slider->parentPanel->y_scroll_offset;
	}
	else
	{
		slider->boundBox.x = slider->x;
		slider->boundBox.y = slider->y;
	}
	
	slider->bar.x = slider->boundBox.x;
	slider->bar.y = slider->boundBox.y + (slider->slider.h / 2) - (slider->bar.h / 2);
	slider->slider_xi = slider->bar.x + (slider->value_i * slider->bar.w);
	slider->slider.x = slider->slider_xi;
	slider->slider.y = slider->bar.y - (slider->slider.h / 2) + (slider->bar.h / 2);
	
	/* Recalculate the parent panel's MCR */
	if(slider->parentPanel != NULL)
	{
		SGE_WindowPanelCalculateMCR(slider->parentPanel, slider->boundBox);
	}
}

void SGE_SliderUpdateValue(SGE_Slider *slider)
{
	slider->value_i = (slider->slider_xi - slider->bar.x) / (double)slider->bar.w;
	/* Limit the value to be between 0 and 1 */
	if(slider->value_i > 1.0f - slider->x_offset)
	{
		slider->value_i = 1.0f - slider->x_offset;
	}
	if(slider->value_i < 0.0f)
	{
		slider->value_i = 0.0f;
	}
	slider->value = slider->value_i * (1 / (1 - slider->x_offset));
}

void SGE_SliderSetValue(SGE_Slider *slider, double value)
{
	slider->value = value;
	/* Limit the value to be between 0 and 1 */
	if(slider->value > 1.0f)
	{
		slider->value = 1.0f;
	}
	if(slider->value < 0.0f)
	{
		slider->value = 0.0f;
	}
	slider->value_i = slider->value / (1 / (1 - slider->x_offset));
	
	/* Recalculate slider position based on value */
	slider->slider_xi = slider->bar.x + (slider->value_i * slider->bar.w);
	slider->slider.x = slider->slider_xi;
}

SGE_TextInputBox *SGE_CreateTextInputBox(int maxTextLength, int x, int y, struct SGE_WindowPanel *panel)
{
	SGE_TextInputBox *textInputBox = NULL;
	textInputBox = (SGE_TextInputBox*)malloc(sizeof(SGE_TextInputBox));
	
	if(panel != NULL)
	{
		if(panel->textInputBoxCount == PANEL_MAX_TEXT_INPUT_BOXES)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_WARNING, "Failed to add TextInputBox! Max amount of TextInputBoxes [%d] in panel %s reached.", PANEL_MAX_TEXT_INPUT_BOXES, panel->titleStr);
			free(textInputBox);
			return NULL;
		}
		/* Add this new textInputBox to the top of the parent panel's textInputBoxes list */
		panel->textInputBoxes[panel->textInputBoxCount] = textInputBox;
		panel->textInputBoxCount += 1;
		panel->controlCount += 1;
		textInputBox->parentPanel = panel;
		textInputBox->alpha = textInputBox->parentPanel->alpha;
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Added Control: %s->TextInputBox %d", panel->titleStr, panel->textInputBoxCount);
	}
	else
	{
		if(currentStateControls->textInputBoxCount == STATE_MAX_TEXT_INPUT_BOXES)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_WARNING, "Failed to add TextInputBox! Max amount of parentless textInputBoxes [%d] reached.", STATE_MAX_TEXT_INPUT_BOXES);
			free(textInputBox);
			return NULL;
		}
		/* Add this new textInputBox to the top of the parentless buttons list */
		currentStateControls->textInputBoxes[currentStateControls->textInputBoxCount] = textInputBox;
		currentStateControls->textInputBoxCount += 1;
		textInputBox->parentPanel = NULL;
		textInputBox->alpha = 255;
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Added Control: {NULL}->TextInputBox %d", currentStateControls->textInputBoxCount);
	}
	
	textInputBox->x = x;
	textInputBox->y = y;
	
	/* Calculate the bounding box based on parent panel position */
	if(textInputBox->parentPanel != NULL)
	{
		textInputBox->boundBox.x = textInputBox->x + textInputBox->parentPanel->background.x + textInputBox->parentPanel->x_scroll_offset;
		textInputBox->boundBox.y = textInputBox->y + textInputBox->parentPanel->background.y + textInputBox->parentPanel->y_scroll_offset;
	}
	else
	{
		textInputBox->boundBox.x = textInputBox->x;
		textInputBox->boundBox.y = textInputBox->y;
	}
	
	textInputBox->textLengthLimit = maxTextLength;
	textInputBox->text = malloc(textInputBox->textLengthLimit * sizeof(char));
	textInputBox->text[0] = '\0';
	
	textInputBox->inputBox.x = textInputBox->boundBox.x;
	textInputBox->inputBox.y = textInputBox->boundBox.y;
	textInputBox->inputBox.w = 250;
	textInputBox->inputBox.h = 100;
	
	SGE_SetTextureWordWrap(textInputBox->inputBox.w);
	
	textInputBox->boundBox.w = textInputBox->inputBox.w;
	textInputBox->boundBox.h = textInputBox->inputBox.h;
	
	textInputBox->textImg = SGE_CreateTextureFromText(" ", textBoxFont, SGE_COLOR_BLACK, SGE_TEXT_MODE_BLENDED);
	textInputBox->textImg->x = textInputBox->inputBox.x + 5;
	textInputBox->textImg->y = textInputBox->inputBox.y + 5;
	
	textInputBox->cursor.x = textInputBox->textImg->x;
	textInputBox->cursor.y = textInputBox->textImg->y + 20;
	textInputBox->cursor.w = 10;
	textInputBox->cursor.h = 5;
	textInputBox->cursor_dx = 0;
	textInputBox->cursor_dy = 0;
	textInputBox->lastTextWidth = 0;
	textInputBox->currentCharWidth = textInputBox->textImg->w;
	textInputBox->characterWidthStack = SGE_LLCreate(NULL);

	textInputBox->showCursor = false;
	textInputBox->lastTime = 0;
	
	SDL_StopTextInput();
	textInputBox->isEnabled = false;
	
	textInputBox->onEnable = onEnableFallback;
	textInputBox->onEnable_data = NULL;
	textInputBox->onDisable = onDisableFallback;
	textInputBox->onDisable_data = NULL;
	textInputBox->onTextEnter = onTextEnterFallback;
	textInputBox->onTextEnter_data = NULL;
	textInputBox->onTextDelete = onTextDeleteFallback;
	textInputBox->onTextDelete_data = NULL;
	
	/* Recalculate the parent panel's MCR */
	if(panel != NULL)
	{
		SGE_WindowPanelCalculateMCR(panel, textInputBox->boundBox);
	}
	
	return textInputBox;
}

void SGE_DestroyTextInputBox(SGE_TextInputBox *textInputBox)
{
	if(textInputBox != NULL)
	{
		SGE_LLDestroy(textInputBox->characterWidthStack);
		free(textInputBox->text);
		SGE_FreeTexture(textInputBox->textImg);
		free(textInputBox);
	}
}

void SGE_TextInputBoxHandleEvents(SGE_TextInputBox *textInputBox)
{
	/* Enable or disable text input with mouse */
	if(engine->event.type == SDL_MOUSEBUTTONDOWN)
	{
		if(SGE_isMouseOver(&textInputBox->inputBox))
		{
			if(textInputBox->parentPanel != NULL)
			{
				if(SGE_isMouseOver(&textInputBox->parentPanel->background) && !SGE_isMouseOver(&textInputBox->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&textInputBox->parentPanel->verticalScrollbarBG))
				{
					if(!textInputBox->isEnabled)
					{
						SDL_StartTextInput();
						textInputBox->isEnabled = true;
						textInputBox->onEnable(textInputBox->onEnable_data);
					}
				}
			}
			else
			{
				if(!textInputBox->isEnabled)
				{
					SDL_StartTextInput();
					textInputBox->isEnabled = true;
					textInputBox->onEnable(textInputBox->onEnable_data);
				}
			}
		}
		else
		{
			if(textInputBox->isEnabled)
			{
				SDL_StopTextInput();
				textInputBox->isEnabled = false;
				textInputBox->onDisable(textInputBox->onDisable_data);
			}
		}
	}

	/* Handle text input events */
	if(!textInputBox->isEnabled)
		return;
	
	switch(engine->event.type)
	{
		case SDL_TEXTINPUT:
		if(strlen(textInputBox->text) < textInputBox->textLengthLimit - 1)
		{
			strcat(textInputBox->text, engine->event.text.text);
			SGE_UpdateTextureFromText(textInputBox->textImg, textInputBox->text, textBoxFont, SGE_COLOR_BLACK, SGE_TEXT_MODE_BLENDED);
			textInputBox->onTextEnter(textInputBox->onTextEnter_data);

			textInputBox->cursor_dx = textInputBox->textImg->w;
			textInputBox->currentCharWidth = textInputBox->textImg->w - textInputBox->lastTextWidth;
			SGE_LLPush(textInputBox->characterWidthStack, textInputBox->currentCharWidth);
			textInputBox->lastTextWidth = textInputBox->textImg->w;
		}
		else
			SGE_LogPrintLine(SGE_LOG_WARNING, "Max characters for textInputBox [%d] reached!", textInputBox->textLengthLimit);
		break;
		
		case SDL_KEYDOWN:
		if(engine->event.key.keysym.sym == SDLK_BACKSPACE)
		{
			int i = strlen(textInputBox->text);
			if(i == 0) {
				return;
			}
			
			textInputBox->text[i - 1] = '\0';
			
			textInputBox->cursor_dx -= textInputBox->currentCharWidth;
			SGE_LLPop(textInputBox->characterWidthStack);

			if(i == 1)
			{
				SGE_UpdateTextureFromText(textInputBox->textImg, " ", textBoxFont, SGE_COLOR_BLACK, SGE_TEXT_MODE_BLENDED);
				textInputBox->currentCharWidth = 0;
				textInputBox->lastTextWidth = 0;
			}
			else
			{
				SGE_UpdateTextureFromText(textInputBox->textImg, textInputBox->text, textBoxFont, SGE_COLOR_BLACK, SGE_TEXT_MODE_BLENDED);
				textInputBox->currentCharWidth = (int) SGE_LLGetLast(textInputBox->characterWidthStack);
				textInputBox->lastTextWidth = textInputBox->textImg->w;
			}
			
			textInputBox->onTextDelete(textInputBox->onTextDelete_data);
		}
		break;
	}
}

void SGE_TextInputBoxUpdate(SGE_TextInputBox *textInputBox)
{
	if(textInputBox->parentPanel != NULL)
	{
		textInputBox->boundBox.x = textInputBox->x + textInputBox->parentPanel->background.x + textInputBox->parentPanel->x_scroll_offset;
		textInputBox->boundBox.y = textInputBox->y + textInputBox->parentPanel->background.y + textInputBox->parentPanel->y_scroll_offset;
		textInputBox->inputBox.x = textInputBox->boundBox.x;
		textInputBox->inputBox.y = textInputBox->boundBox.y;
		textInputBox->textImg->x = textInputBox->inputBox.x + 5;
		textInputBox->textImg->y = textInputBox->inputBox.y + 5;
		textInputBox->cursor.x = textInputBox->textImg->x + textInputBox->cursor_dx;
		textInputBox->cursor.y = textInputBox->textImg->y + 20 + textInputBox->cursor_dy;
		
		textInputBox->alpha = textInputBox->parentPanel->alpha;
		SGE_SetTextureAlpha(textInputBox->textImg, textInputBox->alpha);
	}
	
	if(SDL_GetTicks() - textInputBox->lastTime > 500)
	{
		textInputBox->showCursor = !textInputBox->showCursor;
		textInputBox->lastTime = SDL_GetTicks();
	}
}

void SGE_TextInputBoxRender(SGE_TextInputBox *textInputBox)
{
	if(textInputBox->parentPanel != NULL)
	{
		if(textInputBox->parentPanel->isMinimized)
			return;
	}
	
	SDL_SetRenderDrawColor(engine->renderer, 150, 150, 150, textInputBox->alpha);
	SDL_RenderFillRect(engine->renderer, &textInputBox->inputBox);
	
	if(textInputBox->isEnabled)
	{
		if(textInputBox->showCursor)
		{
			SDL_SetRenderDrawColor(engine->renderer, 150, 0, 0, textInputBox->alpha);
			SDL_RenderFillRect(engine->renderer, &textInputBox->cursor);
			SDL_SetRenderDrawColor(engine->renderer, 255, 255, 255, textInputBox->alpha);
			SDL_RenderDrawRect(engine->renderer, &textInputBox->cursor);
		}
	}
	
	SGE_RenderTexture(textInputBox->textImg);
	
	SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, textInputBox->alpha);
	if(SGE_isMouseOver(&textInputBox->inputBox))
	{
		if(textInputBox->parentPanel != NULL)
		{
			if(SGE_isMouseOver(&textInputBox->parentPanel->background) && !SGE_isMouseOver(&textInputBox->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&textInputBox->parentPanel->verticalScrollbarBG))
				SDL_SetRenderDrawColor(engine->renderer, 255, 255, 255, textInputBox->alpha);
			
			int i = 0;
			for(i = textInputBox->parentPanel->index + 1; i < currentStateControls->panelCount; i++)
			{
				if(SGE_isMouseOver(&currentStateControls->panels[i]->border))
					SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, textInputBox->alpha);
			}
		}
		else
			SDL_SetRenderDrawColor(engine->renderer, 255, 255, 255, textInputBox->alpha);
	}
	SDL_RenderDrawRect(engine->renderer, &textInputBox->inputBox);
	
	if(showControlBounds)
	{
		SDL_SetRenderDrawColor(engine->renderer, controlBoundsColor.r, controlBoundsColor.g, controlBoundsColor.b, textInputBox->alpha);
		SDL_RenderDrawRect(engine->renderer, &textInputBox->textImg->destRect);
		SDL_RenderDrawRect(engine->renderer, &textInputBox->boundBox);
	}
}

void SGE_TextInputBoxSetPosition(SGE_TextInputBox *textInputBox, int x, int y)
{
	textInputBox->x = x;
	textInputBox->y = y;
	
	/* Calculate the bounding box based on parent panel position */
	if(textInputBox->parentPanel != NULL)
	{
		textInputBox->boundBox.x = textInputBox->x + textInputBox->parentPanel->background.x + textInputBox->parentPanel->x_scroll_offset;
		textInputBox->boundBox.y = textInputBox->y + textInputBox->parentPanel->background.y + textInputBox->parentPanel->y_scroll_offset;
	}
	else
	{
		textInputBox->boundBox.x = textInputBox->x;
		textInputBox->boundBox.y = textInputBox->y;
	}
	
	textInputBox->inputBox.x = textInputBox->boundBox.x;
	textInputBox->inputBox.y = textInputBox->boundBox.y;
	textInputBox->textImg->x = textInputBox->inputBox.x + 5;
	textInputBox->textImg->y = textInputBox->inputBox.y + 5;
	textInputBox->cursor.x = textInputBox->textImg->x;
	textInputBox->cursor.y = textInputBox->textImg->y + 20;
	
	/* Recalculate the parent panel's MCR */
	if(textInputBox->parentPanel != NULL)
	{
		SGE_WindowPanelCalculateMCR(textInputBox->parentPanel, textInputBox->boundBox);
	}
}

void SGE_TextInputBoxClear(SGE_TextInputBox *textInputBox)
{
	textInputBox->text[0] = '\0';
	SGE_UpdateTextureFromText(textInputBox->textImg, " ", textBoxFont, SGE_COLOR_BLACK, SGE_TEXT_MODE_BLENDED);
	textInputBox->currentCharWidth = 0;
	textInputBox->lastTextWidth = 0;
	SGE_LLClear(textInputBox->characterWidthStack);
	textInputBox->cursor_dx = 0;
	textInputBox->cursor_dy = 0;
}

SGE_ListBox *SGE_CreateListBox(int listCount, char list[][LIST_OPTION_LENGTH], int x, int y, SGE_WindowPanel *panel)
{
	int i = 0;
	
	SGE_ListBox *listBox = NULL;
	listBox = (SGE_ListBox *)malloc(sizeof(SGE_ListBox));
	
	if(panel != NULL)
	{
		if(panel->listBoxCount == PANEL_MAX_LISTBOXES)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_WARNING, "Failed to add ListBox! Max amount of listBoxes [%d] in panel %s reached.", PANEL_MAX_LISTBOXES, panel->titleStr);
			free(listBox);
			return NULL;
		}
		/* Add this new listBox to the top of the parent panel's listBoxes list */
		panel->listBoxes[panel->listBoxCount] = listBox;
		panel->listBoxCount += 1;
		panel->controlCount += 1;
		listBox->parentPanel = panel;
		listBox->alpha = listBox->parentPanel->alpha;
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Added Control: %s->ListBox %d", panel->titleStr, panel->listBoxCount);
	}
	else
	{
		if(currentStateControls->listBoxCount == STATE_MAX_LISTBOXES)
		{
			SGE_GUI_LogPrintLine(SGE_LOG_WARNING, "Failed to add ListBox! Max amount of parentless listBoxes [%d] reached.", STATE_MAX_LISTBOXES);
			free(listBox);
			return NULL;
		}
		/* Add this new listBox to the top of the parentless listBoxes list */
		currentStateControls->listBoxes[currentStateControls->listBoxCount] = listBox;
		currentStateControls->listBoxCount += 1;
		listBox->parentPanel = NULL;
		listBox->alpha = 255;
		SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Added Control: {NULL}->ListBox %d", currentStateControls->listBoxCount);
	}
	
	listBox->x = x;
	listBox->y = y;
	
	for(i = 0; i < listCount; i++)
	{
		strcpy(listBox->optionList[i], list[i]);
	}
	
	if(listBox->parentPanel != NULL)
	{
		listBox->boundBox.x = listBox->x + listBox->parentPanel->background.x + listBox->parentPanel->x_scroll_offset;
		listBox->boundBox.y = listBox->y + listBox->parentPanel->background.y + listBox->parentPanel->y_scroll_offset;
	}
	else
	{
		listBox->boundBox.x = listBox->x;
		listBox->boundBox.y = listBox->y;
	}
	
	listBox->selectionBox.x = listBox->boundBox.x;
	listBox->selectionBox.y = listBox->boundBox.y;
	listBox->selectionBox.w = 200;
	listBox->selectionBox.h = 25;
	
	listBox->boundBox.w = listBox->selectionBox.w;
	listBox->boundBox.h = listBox->selectionBox.h;
	
	listBox->selection = 0;
	listBox->selectionImg = SGE_CreateTextureFromText(list[listBox->selection], listBoxFont, SGE_COLOR_BLACK, SGE_TEXT_MODE_BLENDED);
	listBox->selectionImg->x = listBox->selectionBox.x + 2;
	listBox->selectionImg->y = listBox->selectionBox.y + 2;
	
	listBox->optionCount = listCount;
	for(i = 0; i < listBox->optionCount; i++)
	{
		listBox->optionBoxes[i].x = listBox->selectionBox.x;
		listBox->optionBoxes[i].y = listBox->selectionBox.y + (i * listBox->selectionBox.h) + listBox->selectionBox.h;
		listBox->optionBoxes[i].w = listBox->selectionBox.w;
		listBox->optionBoxes[i].h = listBox->selectionBox.h;
		
		listBox->optionImages[i] = SGE_CreateTextureFromText(list[i], listBoxFont, SGE_COLOR_BLACK, SGE_TEXT_MODE_BLENDED);
		listBox->optionImages[i]->x = listBox->optionBoxes[i].x + 2;
		listBox->optionImages[i]->y = listBox->optionBoxes[i].y + 2;
	}
	
	listBox->isOpen = false;
	
	listBox->onSelectionChange = onSelectionChangeFallback;
	listBox->onSelectionChange_data = NULL;
	
	/* Recalculate the parent panel's MCR */
	if(panel != NULL)
	{
		SGE_WindowPanelCalculateMCR(panel, listBox->boundBox);
	}
	
	return listBox;
}

void SGE_DestroyListBox(SGE_ListBox *listBox)
{
	int i = 0;
	
	if(listBox != NULL)
	{
		SGE_FreeTexture(listBox->selectionImg);
		for(i = 0; i < listBox->optionCount; i++)
		{
			SGE_FreeTexture(listBox->optionImages[i]);
		}
		free(listBox);
	}
}

void SGE_ListBoxHandleEvents(SGE_ListBox *listBox)
{
	if(engine->event.type == SDL_MOUSEBUTTONDOWN)
	{
		if(SGE_isMouseOver(&listBox->selectionBox))
		{
			if(listBox->parentPanel != NULL)
			{
				if(SGE_isMouseOver(&listBox->parentPanel->background) && !SGE_isMouseOver(&listBox->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&listBox->parentPanel->verticalScrollbarBG))
					listBox->isOpen = !listBox->isOpen;
			}
			else
				listBox->isOpen = !listBox->isOpen;
		}
		
		if(listBox->isOpen)
		{
			int i = 0;
			for(i = 0; i < listBox->optionCount; i++)
			{
				if(SGE_isMouseOver(&listBox->optionBoxes[i]))
				{
					if(listBox->parentPanel != NULL)
					{
						if(SGE_isMouseOver(&listBox->parentPanel->background) && !SGE_isMouseOver(&listBox->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&listBox->parentPanel->verticalScrollbarBG))
						{
							if(i != listBox->selection)
							{
								listBox->selection = i;
								SGE_UpdateTextureFromText(listBox->selectionImg, listBox->optionList[i], listBoxFont, SGE_COLOR_BLACK, SGE_TEXT_MODE_BLENDED);
								listBox->onSelectionChange(listBox->onSelectionChange_data);
							}
							listBox->isOpen = false;
						}
					}
					else
					{
						if(i != listBox->selection)
						{
							listBox->selection = i;
							SGE_UpdateTextureFromText(listBox->selectionImg, listBox->optionList[i], listBoxFont, SGE_COLOR_BLACK, SGE_TEXT_MODE_BLENDED);
							listBox->onSelectionChange(listBox->onSelectionChange_data);
						}
						listBox->isOpen = false;
					}
				}
			}
		}
	}
}

void SGE_ListBoxUpdate(SGE_ListBox *listBox)
{
	if(listBox->parentPanel != NULL)
	{
		int i = 0;
		
		listBox->boundBox.x = listBox->x + listBox->parentPanel->background.x + listBox->parentPanel->x_scroll_offset;
		listBox->boundBox.y = listBox->y + listBox->parentPanel->background.y + listBox->parentPanel->y_scroll_offset;
		listBox->selectionBox.x = listBox->boundBox.x;
		listBox->selectionBox.y = listBox->boundBox.y;
		listBox->selectionImg->x = listBox->selectionBox.x + 2;
		listBox->selectionImg->y = listBox->selectionBox.y + 2;
		listBox->alpha = listBox->parentPanel->alpha;
		SGE_SetTextureAlpha(listBox->selectionImg, listBox->alpha);
		
		listBox->boundBox.h = listBox->selectionBox.h;
		if(listBox->isOpen)
		{
			for(i = 0; i < listBox->optionCount; i++)
			{
				listBox->optionBoxes[i].x = listBox->selectionBox.x;
				listBox->optionBoxes[i].y = listBox->selectionBox.y + (i * listBox->selectionBox.h) + listBox->selectionBox.h;
				listBox->optionImages[i]->x = listBox->optionBoxes[i].x + 2;
				listBox->optionImages[i]->y = listBox->optionBoxes[i].y + 2;
				SGE_SetTextureAlpha(listBox->optionImages[i], listBox->alpha);
				listBox->boundBox.h = listBox->selectionBox.h + (i + 1) * listBox->optionBoxes[i].h;
			}
		}
	}
}

void SGE_ListBoxRender(SGE_ListBox *listBox)
{
	SDL_SetRenderDrawColor(engine->renderer, 255, 255, 255, listBox->alpha);
	SDL_RenderFillRect(engine->renderer, &listBox->selectionBox);
	SGE_RenderTexture(listBox->selectionImg);
	
	SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, listBox->alpha);
	if(SGE_isMouseOver(&listBox->selectionBox))
	{
		if(listBox->parentPanel != NULL)
		{
			if(SGE_isMouseOver(&listBox->parentPanel->background) && !SGE_isMouseOver(&listBox->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&listBox->parentPanel->verticalScrollbarBG))
				SDL_SetRenderDrawColor(engine->renderer, 150, 150, 150, listBox->alpha);
			
			int i = 0;
			for(i = listBox->parentPanel->index + 1; i < currentStateControls->panelCount; i++)
			{
				if(SGE_isMouseOver(&currentStateControls->panels[i]->border))
					SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, listBox->alpha);
			}
		}
		else
			SDL_SetRenderDrawColor(engine->renderer, 150, 150, 150, listBox->alpha);
	}
	SDL_RenderDrawRect(engine->renderer, &listBox->selectionBox);
	
	if(showControlBounds)
	{
		SDL_SetRenderDrawColor(engine->renderer, controlBoundsColor.r, controlBoundsColor.g, controlBoundsColor.b, listBox->alpha);
		SDL_RenderDrawRect(engine->renderer, &listBox->boundBox);
	}
	
	if(listBox->isOpen)
	{
		int i = 0;
		for(i = 0; i < listBox->optionCount; i++)
		{
			SDL_SetRenderDrawColor(engine->renderer, 255, 255, 255, listBox->alpha);
			if(SGE_isMouseOver(&listBox->optionBoxes[i]))
			{
				if(listBox->parentPanel != NULL)
				{
					if(SGE_isMouseOver(&listBox->parentPanel->background) && !SGE_isMouseOver(&listBox->parentPanel->horizontalScrollbarBG) && !SGE_isMouseOver(&listBox->parentPanel->verticalScrollbarBG))
						SDL_SetRenderDrawColor(engine->renderer, 50, 50, 150, listBox->alpha);
					
					int i = 0;
					for(i = listBox->parentPanel->index + 1; i < currentStateControls->panelCount; i++)
					{
						if(SGE_isMouseOver(&currentStateControls->panels[i]->border))
							SDL_SetRenderDrawColor(engine->renderer, 255, 255, listBox->alpha, listBox->alpha);
					}
				}
				else
					SDL_SetRenderDrawColor(engine->renderer, 50, 50, 150, listBox->alpha);
			}
			else
				SDL_SetRenderDrawColor(engine->renderer, 255, 255, 255, listBox->alpha);
			SDL_RenderFillRect(engine->renderer, &listBox->optionBoxes[i]);
			
			SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, listBox->alpha);
			SDL_RenderDrawRect(engine->renderer, &listBox->optionBoxes[i]);
			SGE_RenderTexture(listBox->optionImages[i]);
		}
	}
}

void SGE_ListBoxSetPosition(SGE_ListBox *listBox, int x, int y)
{
	listBox->x = x;
	listBox->y = y;
	
	/* Calculate the bounding box based on parent panel position */
	if(listBox->parentPanel != NULL)
	{
		listBox->boundBox.x = listBox->x + listBox->parentPanel->background.x + listBox->parentPanel->x_scroll_offset;
		listBox->boundBox.y = listBox->y + listBox->parentPanel->background.y + listBox->parentPanel->y_scroll_offset;
	}
	else
	{
		listBox->boundBox.x = listBox->x;
		listBox->boundBox.y = listBox->y;
	}
	
	listBox->selectionBox.x = listBox->boundBox.x;
	listBox->selectionBox.y = listBox->boundBox.y;
	listBox->selectionImg->x = listBox->selectionBox.x + 2;
	listBox->selectionImg->y = listBox->selectionBox.y + 2;
	
	int i = 0;
	for(i = 0; i < listBox->optionCount; i++)
	{
		listBox->optionBoxes[i].x = listBox->selectionBox.x;
		listBox->optionBoxes[i].y = listBox->selectionBox.y + (i * listBox->selectionBox.h) + listBox->selectionBox.h;
		listBox->optionImages[i]->x = listBox->optionBoxes[i].x + 2;
		listBox->optionImages[i]->y = listBox->optionBoxes[i].y + 2;
	}
	
	/* Recalculate the parent panel's MCR */
	if(listBox->parentPanel != NULL)
	{
		SGE_WindowPanelCalculateMCR(listBox->parentPanel, listBox->boundBox);
	}
}

SGE_MinimizeButton *SGE_CreateMinimizeButton(SGE_WindowPanel *panel)
{
	SGE_MinimizeButton *minimizeButton = (SGE_MinimizeButton *) malloc(sizeof(SGE_MinimizeButton));
	minimizeButton->parentPanel = panel;
	minimizeButton->buttonImg = SGE_LoadTexture("assets/minimize_icon.png");
	
	minimizeButton->boundBox.w = minimizeButton->buttonImg->w + 5;
	minimizeButton->boundBox.h = minimizeButton->buttonImg->h + 5;
	minimizeButton->boundBox.x = panel->background.x;
	minimizeButton->boundBox.y = panel->titleRect.y + (panel->titleRect.h / 2) - (minimizeButton->boundBox.h / 2);
	minimizeButton->buttonImg->x = minimizeButton->boundBox.x + (minimizeButton->boundBox.w / 2) - (minimizeButton->buttonImg->w / 2);
	minimizeButton->buttonImg->y = minimizeButton->boundBox.y + (minimizeButton->boundBox.h / 2) - (minimizeButton->buttonImg->h / 2);
	
	minimizeButton->state = SGE_CONTROL_STATE_NORMAL;
	minimizeButton->normalColor = SGE_COLOR_DARK_RED;
	minimizeButton->hoverColor = SGE_COLOR_GRAY;
	minimizeButton->clickedColor = SGE_COLOR_BLACK;
	minimizeButton->currentColor = minimizeButton->normalColor;
	
	return minimizeButton;
}

void SGE_DestroyMinimizeButton(SGE_MinimizeButton *minButton)
{
	if(minButton != NULL)
	{
		SGE_FreeTexture(minButton->buttonImg);
		free(minButton);
	}
}

void SGE_MinimizeButtonHandleEvents(SGE_MinimizeButton *minButton)
{
	int i = 0;
	
	if(engine->event.type == SDL_MOUSEBUTTONDOWN)
	{
		if(engine->event.button.button == 1)
		{
			if(SGE_isMouseOver(&minButton->boundBox))
			{
				minButton->state = SGE_CONTROL_STATE_CLICKED;
				for(i = minButton->parentPanel->index + 1; i < currentStateControls->panelCount; i++)
				{
					if(SGE_isMouseOver(&currentStateControls->panels[i]->border))
						minButton->state = SGE_CONTROL_STATE_NORMAL;
				}
			}
		}
	}
	else if(engine->event.type == SDL_MOUSEBUTTONUP)
	{
		if(engine->event.button.button == 1)
		{
			if(minButton->state == SGE_CONTROL_STATE_CLICKED)
			{
				if(SGE_isMouseOver(&minButton->boundBox))
				{
					minButton->state = SGE_CONTROL_STATE_HOVER;
					SGE_WindowPanelToggleMinimized(minButton->parentPanel);
				}
				else
				{
					minButton->state = SGE_CONTROL_STATE_NORMAL;
				}
			}
		}
	}
	else if(engine->event.type == SDL_MOUSEMOTION)
	{
		if(minButton->state != SGE_CONTROL_STATE_CLICKED)
		{
			if(SGE_isMouseOver(&minButton->boundBox))
			{
				minButton->state = SGE_CONTROL_STATE_HOVER;
				for(i = minButton->parentPanel->index + 1; i < currentStateControls->panelCount; i++)
				{
					if(SGE_isMouseOver(&currentStateControls->panels[i]->border))
						minButton->state = SGE_CONTROL_STATE_NORMAL;
				}
			}
			else
				minButton->state = SGE_CONTROL_STATE_NORMAL;
		}
	}
}

void SGE_MinimizeButtonUpdate(SGE_MinimizeButton *minButton)
{
	if(minButton->state == SGE_CONTROL_STATE_NORMAL)
		minButton->currentColor = minButton->normalColor;
	else if(minButton->state == SGE_CONTROL_STATE_HOVER)
		minButton->currentColor = minButton->hoverColor;
	else if(minButton->state == SGE_CONTROL_STATE_CLICKED)
		minButton->currentColor = minButton->clickedColor;
}

void SGE_MinimizeButtonRender(SGE_MinimizeButton *minButton)
{
	int i = 0;
	
	SDL_SetRenderDrawColor(engine->renderer, minButton->currentColor.r, minButton->currentColor.g, minButton->currentColor.b, minButton->parentPanel->alpha);
	SDL_RenderFillRect(engine->renderer, &minButton->boundBox);
	
	/* Draw button image */
	SGE_SetTextureAlpha(minButton->buttonImg, minButton->parentPanel->alpha);
	SGE_RenderTexture(minButton->buttonImg);
	
	/* Draw button border */
	SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, minButton->parentPanel->alpha);
	if(SGE_isMouseOver(&minButton->boundBox))
	{
		SDL_SetRenderDrawColor(engine->renderer, 225, 225, 225, minButton->parentPanel->alpha);
		
		for(i = minButton->parentPanel->index + 1; i < currentStateControls->panelCount; i++)
		{
			if(SGE_isMouseOver(&currentStateControls->panels[i]->border))
				SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, minButton->parentPanel->alpha);
		}
	}
	SDL_RenderDrawRect(engine->renderer, &minButton->boundBox);
}

SGE_WindowPanel *SGE_CreateWindowPanel(const char *title, int x, int y, int w, int h) 
{
	int i = 0;

	SGE_WindowPanel **panels = currentStateControls->panels;
	int panelCount = currentStateControls->panelCount;
	
	if(panelCount == STATE_MAX_PANELS)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_WARNING, "Failed to create panel %s! Max amount of Panels [%d] reached.", title, STATE_MAX_PANELS);
		return NULL;
	}
	
	/* Create the new panel, add it to the top of the panels stack and set it as the active window */
	SGE_WindowPanel *panel = NULL;
	panel = (SGE_WindowPanel *)malloc(sizeof(SGE_WindowPanel));
	
	panel->index = currentStateControls->panelCount;
	panels[currentStateControls->panelCount] = panel;
	currentStateControls->panelCount += 1;
	
	strncpy(panel->titleStr, title, 50);
	SGE_SetActiveWindowPanel(panel);
	SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Added Panel: %s", panel->titleStr);
	
	panel->titleTextImg = SGE_CreateTextureFromText(title, panelTitleFont, SGE_COLOR_WHITE, SGE_TEXT_MODE_BLENDED);
	if(panel->titleTextImg == NULL)
	{
		SGE_GUI_LogPrintLine(SGE_LOG_ERROR, "Failed to load render panel title text!");
		free(panel);
		return NULL;
	}
	
	panel->isVisible = true;
	panel->backgroundColor = SGE_COLOR_LIGHT_GRAY;
	panel->borderColor = SGE_COLOR_DARK_RED;
	panel->alpha = 255;
	panel->borderThickness = 5;
	panel->titleHeight = 30;
	
	panel->boundBox.x = x;
	panel->boundBox.y = y;
	panel->background.w = w;
	panel->background.h = h;
	
	/* Calculate the panel position and size */
	panel->border.x = panel->boundBox.x;
	panel->border.y = panel->boundBox.y;
	
	panel->border.w = panel->background.w + (2 * panel->borderThickness);
	panel->border.h = panel->background.h + (2 * panel->borderThickness) + panel->titleHeight - panel->borderThickness;
	
	panel->boundBox.w = panel->border.w;
	panel->boundBox.h = panel->border.h;
	
	panel->background.x = panel->border.x + panel->borderThickness;
	panel->background.y = panel->border.y + panel->borderThickness + panel->titleHeight - panel->borderThickness;
	
	panel->titleRect.x = panel->border.x;
	panel->titleRect.y = panel->border.y;
	panel->titleRect.w = panel->border.w;
	panel->titleRect.h = panel->titleHeight + panel->borderThickness / 2;
	
	panel->titleTextImg->x = panel->titleRect.x + (panel->titleRect.w / 2) - (panel->titleTextImg->w / 2);
	panel->titleTextImg->y = panel->titleRect.y + (panel->titleRect.h / 2) - (panel->titleTextImg->h / 2);
	
	panel->bgLocalCenter.x = panel->background.w / 2;
	panel->bgLocalCenter.y = panel->background.h / 2;
	panel->bgGlobalCenter.x = panel->bgLocalCenter.x + panel->background.x;
	panel->bgGlobalCenter.y = panel->bgLocalCenter.y + panel->background.y;
	
	panel->isMovable = true;
	panel->isMoving = false;
	panel->move_dx = 0, panel->move_dy = 0;
	
	panel->isResizable = true;
	panel->isResizing_vertical = false;
	panel->isResizing_horizontal = false;
	panel->resize_origin_x = 0, panel->resize_origin_y = 0;
	panel->resize_origin_w = 0, panel->resize_origin_h = 0;
	
	panel->resizeBar_vertical.w = 30;
	panel->resizeBar_vertical.h = 10;
	panel->resizeBar_vertical.x = panel->border.x + panel->border.w - panel->resizeBar_vertical.w;
	panel->resizeBar_vertical.y = panel->border.y + panel->border.h - panel->resizeBar_vertical.h;
	
	panel->resizeBar_horizontal.w = 10;
	panel->resizeBar_horizontal.h = 30;
	panel->resizeBar_horizontal.x = panel->border.x + panel->border.w - panel->resizeBar_horizontal.w;
	panel->resizeBar_horizontal.y = panel->border.y + panel->border.h - panel->resizeBar_horizontal.h;
	
	panel->minimizeButton = SGE_CreateMinimizeButton(panel);
	panel->isMinimizable = true;
	panel->isMinimized = false;
	panel->temp_border_w = 0;
	panel->temp_border_h = 0;
	panel->temp_background_w = 0;
	panel->temp_background_h = 0;
	
	panel->masterControlRect.x = 0;
	panel->masterControlRect.y = 0;
	panel->masterControlRect.w = 0;
	panel->masterControlRect.h = 0;
	
	panel->temp_horizontalScrollbarEnabled = false;
	panel->temp_verticalScrollbarEnabled = false;
	
	panel->isScrolling_horizontal = false;
	panel->scroll_dx = 0;
	panel->horizontalScrollbar_move_dx = 0;
	panel->x_scroll_offset = 0;
	
	panel->horizontalScrollbarEnabled = false;
	panel->horizontalScrollbarBG.x = panel->background.x;
	panel->horizontalScrollbarBG.h = panel->titleHeight - 10;
	panel->horizontalScrollbarBG.w = panel->background.w - panel->horizontalScrollbarBG.h;
	panel->horizontalScrollbarBG.y = panel->background.y + panel->background.h - panel->horizontalScrollbarBG.h;
	panel->horizontalScrollbar.x = panel->horizontalScrollbarBG.x;
	panel->horizontalScrollbar.y = panel->horizontalScrollbarBG.y;
	panel->horizontalScrollbar.w = panel->horizontalScrollbarBG.w;
	panel->horizontalScrollbar.h = panel->horizontalScrollbarBG.h;
	
	panel->isScrolling_vertical = false;
	panel->scroll_dy = 0;
	panel->verticalScrollbar_move_dy = 0;
	panel->y_scroll_offset = 0;

	panel->verticalScrollbarEnabled = false;
	panel->verticalScrollbarBG.y = panel->background.y;
	panel->verticalScrollbarBG.w = panel->titleHeight - 10;
	panel->verticalScrollbarBG.h = panel->background.h - panel->verticalScrollbarBG.w;
	panel->verticalScrollbarBG.x = panel->background.x + panel->background.w - panel->verticalScrollbarBG.w;
	panel->verticalScrollbar.x = panel->verticalScrollbarBG.x;
	panel->verticalScrollbar.y = panel->verticalScrollbarBG.y;
	panel->verticalScrollbar.w = panel->verticalScrollbarBG.w;
	panel->verticalScrollbar.h = panel->verticalScrollbarBG.h;
	
	panel->onResize = onResizeFallback;
	panel->onResize_data = NULL;
	panel->onMove = onMoveFallback;
	panel->onMove_data = NULL;
	panel->onMinimize = onMinimizeFallback;
	panel->onMinimize_data = NULL;
	panel->onMaximize = onMaximizeFallback;
	panel->onMaximize_data = NULL;
	
	panel->controlCount = 0;
	panel->buttonCount = 0;
	panel->checkBoxCount = 0;
	panel->textLabelCount = 0;
	panel->sliderCount = 0;
	panel->textInputBoxCount = 0;
	panel->listBoxCount = 0;
	
	return panel;
}

void SGE_DestroyWindowPanel(SGE_WindowPanel *panel)
{
	if(panel != NULL)
	{
		SGE_DestroyMinimizeButton(panel->minimizeButton);
		SGE_FreeTexture(panel->titleTextImg);
		free(panel);
	}
}

void SGE_WindowPanelHandleEvents(SGE_WindowPanel *panel)
{
	int i = 0;

	SGE_WindowPanel **panels = currentStateControls->panels;
	int panelCount = currentStateControls->panelCount;
	
	if(panel->isMinimizable)
	{
		SGE_MinimizeButtonHandleEvents(panel->minimizeButton);
	}
	
	/* Handle left mouse button click for panel*/
	if(engine->event.type == SDL_MOUSEBUTTONDOWN)
	{
		if(engine->event.button.button == 1)
		{
			/* Set panel as active when clicked */
			if(SGE_isMouseOver(&panel->border))
			{
				SGE_WindowPanel *active = NULL;
				for(i = 0; i < panelCount; i++)
				{
					if(SGE_isMouseOver(&panels[i]->border))
						active = panels[i];
				}
				SGE_SetActiveWindowPanel(active);
			}
			else
			{
				panel->isActive = false;
				printPanelsStr();
			}
			
			/* Move the panel when titleRect is clicked */
			if(SGE_isMouseOver(&panel->titleRect))
			{
				if(panel->isMovable)
				{
					panel->isMoving = true;
					for(i = panel->index + 1; i < panelCount; i++)
					{
						if(SGE_isMouseOver(&panels[i]->border))
							panel->isMoving = false;
					}
					
					panel->move_dx = engine->mouse_x - panel->border.x;
					panel->move_dy = engine->mouse_y - panel->border.y;

					if(panel->isMinimizable)
					{
						if(SGE_isMouseOver(&panel->minimizeButton->boundBox))
						{
							panel->isMoving = false;
						}
					}
				}
			}
			
			/* Resize the panel vertically */
			if(SGE_isMouseOver(&panel->resizeBar_vertical) && !SGE_isMouseOver(&panel->background))
			{
				if(panel->isResizable && !panel->isMinimized)
				{
					panel->isResizing_vertical = true;
					for(i = panel->index + 1; i < panelCount; i++)
					{
						if(SGE_isMouseOver(&panels[i]->border))
							panel->isResizing_vertical = false;
					}
					
					panel->resize_origin_y = engine->mouse_y;
					panel->resize_origin_h = panel->border.h;
				}
			}
			
			/* Resize the panel horizontally */
			if(SGE_isMouseOver(&panel->resizeBar_horizontal) && !SGE_isMouseOver(&panel->background))
			{
				if(panel->isResizable && !panel->isMinimized)
				{
					panel->isResizing_horizontal = true;
					for(i = panel->index + 1; i < panelCount; i++)
					{
						if(SGE_isMouseOver(&panels[i]->border))
							panel->isResizing_horizontal = false;
					}
					
					panel->resize_origin_x = engine->mouse_x;
					panel->resize_origin_w = panel->border.w;
				}
			}
			
			/* Scroll Horizontally */
			if(panel->horizontalScrollbarEnabled)
			{
				if(SGE_isMouseOver(&panel->horizontalScrollbar))
				{
					panel->isScrolling_horizontal = true;
					for(i = panel->index + 1; i < panelCount; i++)
					{
						if(SGE_isMouseOver(&panels[i]->border))
							panel->isScrolling_horizontal = false;
					}
					panel->horizontalScrollbar_move_dx = engine->mouse_x - panel->horizontalScrollbar.x;
				}
				
				if(SGE_isMouseOver(&panel->horizontalScrollbarBG) && !SGE_isMouseOver(&panel->horizontalScrollbar))
				{
					panel->isScrolling_horizontal = true;
					for(i = panel->index + 1; i < panelCount; i++)
					{
						if(SGE_isMouseOver(&panels[i]->border))
							panel->isScrolling_horizontal = false;
					}
					if(panel->isScrolling_horizontal)
					{
						panel->horizontalScrollbar.x = engine->mouse_x - (panel->horizontalScrollbar.w / 2);
						panel->horizontalScrollbar_move_dx = engine->mouse_x - panel->horizontalScrollbar.x;
					}
				}
			}
			
			/* Scroll Vertically */
			if(panel->verticalScrollbarEnabled)
			{
				if(SGE_isMouseOver(&panel->verticalScrollbar))
				{
					panel->isScrolling_vertical = true;
					for(i = panel->index + 1; i < panelCount; i++)
					{
						if(SGE_isMouseOver(&panels[i]->border))
							panel->isScrolling_vertical = false;
					}
					panel->verticalScrollbar_move_dy = engine->mouse_y - panel->verticalScrollbar.y;
				}
				
				if(SGE_isMouseOver(&panel->verticalScrollbarBG) && !SGE_isMouseOver(&panel->verticalScrollbar))
				{
					panel->isScrolling_vertical = true;
					for(i = panel->index + 1; i < panelCount; i++)
					{
						if(SGE_isMouseOver(&panels[i]->border))
							panel->isScrolling_vertical = false;
					}
					if(panel->isScrolling_vertical)
					{
						panel->verticalScrollbar.y = engine->mouse_y - (panel->verticalScrollbar.h / 2);
						panel->verticalScrollbar_move_dy = engine->mouse_y - panel->verticalScrollbar.y;
					}
				}
			}
		}
	}
	
	else if(engine->event.type == SDL_MOUSEBUTTONUP)
	{
		if(panel->isMoving)
		{
			panel->isMoving = false;
		}
		
		if(panel->isResizing_vertical)
		{
			panel->isResizing_vertical = false;
		}
		
		if(panel->isResizing_horizontal)
		{
			panel->isResizing_horizontal = false;
		}
		
		if(panel->isScrolling_horizontal)
		{
			panel->isScrolling_horizontal = false;
		}
		
		if(panel->isScrolling_vertical)
		{
			panel->isScrolling_vertical = false;
		}
	}
	
	else if(engine->event.type == SDL_MOUSEMOTION)
	{
		if(panel->isMoving)
			panel->onMove(panel->onMove_data);
		if(panel->isResizing_vertical || panel->isResizing_horizontal)
			panel->onResize(panel->onResize_data);
	}
}

void SGE_WindowPanelUpdate(SGE_WindowPanel *panel)
{
	/* Recalculate the window position when moved */
	if(panel->isMoving)
	{
		SGE_WindowPanelSetPosition(panel, engine->mouse_x - panel->move_dx, engine->mouse_y - panel->move_dy);
	}
	
	/* Recalculate the window width when resized horizontally */
	if(panel->isResizing_horizontal)
	{
		panel->isMoving = false;
		panel->border.w = panel->resize_origin_w - (panel->resize_origin_x - engine->mouse_x);
		if(panel->border.w < panel->titleTextImg->w + panel->minimizeButton->boundBox.w + 50)
		{
			panel->border.w = panel->titleTextImg->w + panel->minimizeButton->boundBox.w + 50;
		}
		panel->boundBox.w = panel->border.w;
		
		panel->resizeBar_horizontal.x = panel->border.x + panel->border.w - panel->resizeBar_horizontal.w;
		panel->resizeBar_vertical.x = panel->border.x + panel->border.w - panel->resizeBar_vertical.w;
		
		panel->background.w = panel->border.w - (2 * panel->borderThickness);
		panel->titleRect.w = panel->border.w;
		panel->titleTextImg->x = panel->titleRect.x + (panel->titleRect.w / 2) - (panel->titleTextImg->w / 2);
		
		panel->bgLocalCenter.x = panel->background.w / 2;
		panel->bgGlobalCenter.x = panel->bgLocalCenter.x + panel->background.x;
		
		panel->verticalScrollbarBG.x = panel->background.x + panel->background.w - panel->verticalScrollbarBG.w;
		panel->verticalScrollbar.x = panel->verticalScrollbarBG.x;
		
		SGE_WindowPanelShouldEnableHorizontalScroll(panel);
		SGE_WindowPanelShouldEnableVerticalScroll(panel);
	}
	
	/* Recalculate the window height when resized vertically */
	if(panel->isResizing_vertical)
	{
		panel->isMoving = false;
		panel->border.h = panel->resize_origin_h - (panel->resize_origin_y - engine->mouse_y);
		if(panel->border.h < panel->titleRect.h + panel->horizontalScrollbarBG.h)
		{
			panel->border.h = panel->titleRect.h + panel->horizontalScrollbarBG.h;
		}
		
		panel->boundBox.h = panel->border.h;
		panel->background.h = panel->border.h - (2 * panel->borderThickness) - panel->titleHeight + panel->borderThickness;
		panel->resizeBar_horizontal.y = panel->border.y + panel->border.h - panel->resizeBar_horizontal.h;
		panel->resizeBar_vertical.y = panel->border.y + panel->border.h - panel->resizeBar_vertical.h;
		
		panel->titleRect.h = panel->titleHeight + panel->borderThickness / 2;
		panel->titleTextImg->y = panel->titleRect.y + (panel->titleRect.h / 2) - (panel->titleTextImg->h / 2);
		
		panel->bgLocalCenter.y = panel->background.h / 2;
		panel->bgGlobalCenter.y = panel->bgLocalCenter.y + panel->background.y;
		
		panel->horizontalScrollbarBG.y = panel->background.y + panel->background.h - panel->horizontalScrollbarBG.h;
		panel->horizontalScrollbar.y = panel->horizontalScrollbarBG.y;
		
		SGE_WindowPanelShouldEnableHorizontalScroll(panel);
		SGE_WindowPanelShouldEnableVerticalScroll(panel);
	}
	
	if(panel->isScrolling_horizontal)
	{
		panel->horizontalScrollbar.x = engine->mouse_x - panel->horizontalScrollbar_move_dx;
		if(panel->horizontalScrollbar.x < panel->horizontalScrollbarBG.x)
			panel->horizontalScrollbar.x = panel->horizontalScrollbarBG.x;
		if(panel->horizontalScrollbar.x + panel->horizontalScrollbar.w > panel->horizontalScrollbarBG.x + panel->horizontalScrollbarBG.w)
			panel->horizontalScrollbar.x = panel->horizontalScrollbarBG.x + panel->horizontalScrollbarBG.w - panel->horizontalScrollbar.w;
		
		int temp_w = panel->masterControlRect.w + (panel->masterControlRect.x - panel->background.x);
		panel->x_scroll_offset = ((panel->horizontalScrollbarBG.x - panel->horizontalScrollbar.x) / (double)panel->horizontalScrollbarBG.w) * temp_w;
	}
	
	if(panel->isScrolling_vertical)
	{
		panel->verticalScrollbar.y = engine->mouse_y - panel->verticalScrollbar_move_dy;
		if(panel->verticalScrollbar.y < panel->verticalScrollbarBG.y)
			panel->verticalScrollbar.y = panel->verticalScrollbarBG.y;
		if(panel->verticalScrollbar.y + panel->verticalScrollbar.h > panel->verticalScrollbarBG.y + panel->verticalScrollbarBG.h)
			panel->verticalScrollbar.y = panel->verticalScrollbarBG.y + panel->verticalScrollbarBG.h - panel->verticalScrollbar.h;
		
		int temp_h = panel->masterControlRect.h + (panel->masterControlRect.y - panel->background.y);
		panel->y_scroll_offset = ((panel->verticalScrollbarBG.y - panel->verticalScrollbar.y) / (double)panel->verticalScrollbarBG.h) * temp_h;
	}
	
	if(panel->isMinimizable)
	{
		SGE_MinimizeButtonUpdate(panel->minimizeButton);
	}
}

void SGE_WindowPanelRender(SGE_WindowPanel *panel)
{
	int i = 0;
	
	/* Draw a rect that acts as a border and title bar */
	SDL_SetRenderDrawBlendMode(engine->renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(engine->renderer, panel->borderColor.r, panel->borderColor.g, panel->borderColor.b, panel->alpha);
	SDL_RenderFillRect(engine->renderer, &panel->border);
	
	/* Draw a white or black border around the panel */
	if(panel->isActive)
	{
		SDL_SetRenderDrawColor(engine->renderer, 255, 255, 255, panel->alpha);
	}
	else
	{
		SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, panel->alpha);
	}
	SDL_RenderDrawRect(engine->renderer, &panel->border);
	
	/* Draw the actual background of the panel */
	SDL_SetRenderDrawColor(engine->renderer, panel->backgroundColor.r, panel->backgroundColor.g, panel->backgroundColor.b, panel->alpha);
	SDL_RenderFillRect(engine->renderer, &panel->background);
	
	/* Draw the panel title text */
	SGE_SetTextureAlpha(panel->titleTextImg, panel->alpha);
	SGE_RenderTexture(panel->titleTextImg);
	
	if(panel->isMinimizable)
	{
		SGE_MinimizeButtonRender(panel->minimizeButton);
	}
	
	/* Draw all the child controls */
	SDL_RenderSetClipRect(engine->renderer, &panel->background);
	
	for(i = 0; i < panel->buttonCount; i++)
	{
		SGE_ButtonRender(panel->buttons[i]);
	}
	
	for(i = 0; i < panel->checkBoxCount; i++)
	{
		SGE_CheckBoxRender(panel->checkBoxes[i]);
	}
	
	for(i = 0; i < panel->textLabelCount; i++)
	{
		SGE_TextLabelRender(panel->textLabels[i]);
	}

	for(i = 0; i < panel->sliderCount; i++)
	{
		SGE_SliderRender(panel->sliders[i]);
	}
	
	for(i = 0; i < panel->textInputBoxCount; i++)
	{
		SGE_TextInputBoxRender(panel->textInputBoxes[i]);
	}
	
	for(i = 0; i < panel->listBoxCount; i++)
	{
		SGE_ListBoxRender(panel->listBoxes[i]);
	}
	
	SDL_RenderSetClipRect(engine->renderer, NULL);
	SDL_SetRenderDrawColor(engine->renderer, 255, 255, 255, 0);
	SDL_RenderDrawPoint(engine->renderer, 0, 0);
	
	/* Draw Horizontal Scrollbar */
	if(panel->horizontalScrollbarEnabled)
	{
		SDL_SetRenderDrawColor(engine->renderer, 255, 255, 255, panel->alpha);
		SDL_RenderFillRect(engine->renderer, &panel->horizontalScrollbarBG);
		SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, panel->alpha);
		SDL_RenderDrawRect(engine->renderer, &panel->horizontalScrollbarBG);
		
		SDL_SetRenderDrawColor(engine->renderer, panel->borderColor.r, panel->borderColor.g, panel->borderColor.b, panel->alpha);
		SDL_RenderFillRect(engine->renderer, &panel->horizontalScrollbar);
		
		SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, panel->alpha);
		if(SGE_isMouseOver(&panel->horizontalScrollbar))
		{
			SDL_SetRenderDrawColor(engine->renderer, 225, 225, 225, panel->alpha);
			
			for(i = panel->index + 1; i < currentStateControls->panelCount; i++)
			{
				if(SGE_isMouseOver(&currentStateControls->panels[i]->border))
					SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, panel->alpha);
			}
		}
		SDL_RenderDrawRect(engine->renderer, &panel->horizontalScrollbar);
	}
	
	/* Draw Vertical Scrollbar */
	if(panel->verticalScrollbarEnabled)
	{
		SDL_SetRenderDrawColor(engine->renderer, 255, 255, 255, panel->alpha);
		SDL_RenderFillRect(engine->renderer, &panel->verticalScrollbarBG);
		SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, panel->alpha);
		SDL_RenderDrawRect(engine->renderer, &panel->verticalScrollbarBG);
		
		SDL_SetRenderDrawColor(engine->renderer, panel->borderColor.r, panel->borderColor.g, panel->borderColor.b, panel->alpha);
		SDL_RenderFillRect(engine->renderer, &panel->verticalScrollbar);
		
		SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, panel->alpha);
		if(SGE_isMouseOver(&panel->verticalScrollbar))
		{
			SDL_SetRenderDrawColor(engine->renderer, 225, 225, 225, panel->alpha);
			
			for(i = panel->index + 1; i < currentStateControls->panelCount; i++)
			{
				if(SGE_isMouseOver(&currentStateControls->panels[i]->border))
					SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, panel->alpha);
			}
		}
		SDL_RenderDrawRect(engine->renderer, &panel->verticalScrollbar);
	}
	
	if(showControlBounds)
	{
		/* Draw Resize Control Bars */
		SDL_SetRenderDrawColor(engine->renderer, 255, 0, 255, panel->alpha);
		SDL_RenderDrawRect(engine->renderer, &panel->resizeBar_horizontal);
		SDL_SetRenderDrawColor(engine->renderer, 0, 255, 0, panel->alpha);
		SDL_RenderDrawRect(engine->renderer, &panel->resizeBar_vertical);
		
		/* Draw the panel center point */
		SDL_Rect centerRect = {panel->bgGlobalCenter.x - 2, panel->bgGlobalCenter.y - 2, 4, 4};
		SDL_SetRenderDrawColor(engine->renderer, 255, 255, 255, panel->alpha);
		SDL_RenderFillRect(engine->renderer, &centerRect);
		SDL_SetRenderDrawColor(engine->renderer, 0, 0, 0, panel->alpha);
		SDL_RenderDrawRect(engine->renderer, &centerRect);
		
		/* Draw panel MCR */
		SDL_SetRenderDrawColor(engine->renderer, 0, 255, 0, panel->alpha);
		SDL_RenderDrawRect(engine->renderer, &panel->masterControlRect);
		
		/* Draw panel boundbox */
		SDL_SetRenderDrawColor(engine->renderer, 255, 0, 255, panel->alpha);
		SDL_RenderDrawRect(engine->renderer, &panel->boundBox);
	}
}

void SGE_WindowPanelSetPosition(SGE_WindowPanel *panel, int x, int y)
{
	/* Store the difference between new and old positions */
	int dx = x - panel->boundBox.x;
	int dy = y - panel->boundBox.y;
	
	panel->boundBox.x = x;
	panel->boundBox.y = y;
	panel->border.x = panel->boundBox.x;
	panel->border.y = panel->boundBox.y;
	panel->background.x = panel->border.x + panel->borderThickness;
	panel->background.y = panel->border.y + panel->borderThickness + panel->titleHeight - panel->borderThickness;
	panel->titleRect.x = panel->border.x;
	panel->titleRect.y = panel->border.y;
	panel->titleTextImg->x = panel->titleRect.x + (panel->titleRect.w / 2) - (panel->titleTextImg->w / 2);
	panel->titleTextImg->y = panel->titleRect.y + (panel->titleRect.h / 2) - (panel->titleTextImg->h / 2);
	
	panel->resizeBar_horizontal.x = panel->border.x + panel->border.w - panel->resizeBar_horizontal.w;
	panel->resizeBar_horizontal.y = panel->border.y + panel->border.h - panel->resizeBar_horizontal.h;
	panel->resizeBar_vertical.x = panel->border.x + panel->border.w - panel->resizeBar_vertical.w;
	panel->resizeBar_vertical.y = panel->border.y + panel->border.h - panel->resizeBar_vertical.h;
	
	panel->bgLocalCenter.x = panel->background.w / 2;
	panel->bgLocalCenter.y = panel->background.h / 2;
	panel->bgGlobalCenter.x = panel->bgLocalCenter.x + panel->background.x;
	panel->bgGlobalCenter.y = panel->bgLocalCenter.y + panel->background.y;
	
	panel->minimizeButton->boundBox.x = panel->background.x;
	panel->minimizeButton->boundBox.y = panel->titleRect.y + (panel->titleRect.h / 2) - (panel->minimizeButton->boundBox.h / 2);
	panel->minimizeButton->buttonImg->x = panel->minimizeButton->boundBox.x + (panel->minimizeButton->boundBox.w / 2) - (panel->minimizeButton->buttonImg->w / 2);
	panel->minimizeButton->buttonImg->y = panel->minimizeButton->boundBox.y + (panel->minimizeButton->boundBox.h / 2) - (panel->minimizeButton->buttonImg->h / 2);
	
	panel->masterControlRect.x += dx;
	panel->masterControlRect.y += dy;
	
	panel->horizontalScrollbarBG.x = panel->background.x;
	panel->horizontalScrollbarBG.y = panel->background.y + panel->background.h - panel->horizontalScrollbarBG.h;
	panel->horizontalScrollbar.y = panel->horizontalScrollbarBG.y;

	/* Calculate horizontal scrollbar position based on scroll offset */
	int temp_w = panel->masterControlRect.w + (panel->masterControlRect.x - panel->background.x);
	double scrlBarPos = panel->horizontalScrollbarBG.x - ((panel->x_scroll_offset / temp_w) * panel->horizontalScrollbarBG.w);
	/* Must round result and cast to int */
	panel->horizontalScrollbar.x = SDL_ceil(scrlBarPos);

	panel->verticalScrollbarBG.y = panel->background.y;
	panel->verticalScrollbarBG.x = panel->background.x + panel->background.w - panel->verticalScrollbarBG.w;
	panel->verticalScrollbar.x = panel->verticalScrollbarBG.x;

	/* Calculate vertical scrollbar posiotion based on scroll offset */
	int temp_h = panel->masterControlRect.h + (panel->masterControlRect.y - panel->background.y);
	scrlBarPos = panel->verticalScrollbarBG.y - ((panel->y_scroll_offset / (double)temp_h) * panel->verticalScrollbarBG.h);	
	panel->verticalScrollbar.y = SDL_ceil(scrlBarPos);
}

void SGE_WindowPanelSetSize(SGE_WindowPanel *panel, int w, int h)
{
	panel->background.w = w;
	panel->background.h = h; 
	
	panel->border.w = panel->background.w + (2 * panel->borderThickness);
	panel->border.h = panel->background.h + (2 * panel->borderThickness)  + panel->titleHeight - panel->borderThickness;
	panel->boundBox.w = panel->border.w;
	panel->boundBox.h = panel->border.h;
	
	if(panel->border.w < panel->titleTextImg->w + panel->minimizeButton->boundBox.w + 50)
	{
		panel->border.w = panel->titleTextImg->w + panel->minimizeButton->boundBox.w + 50;
	}
	
	if(panel->border.h < panel->titleRect.h + panel->horizontalScrollbarBG.h)
	{
		panel->border.h = panel->titleRect.h + panel->horizontalScrollbarBG.h;
	}
	
	panel->titleRect.w = panel->border.w;
	panel->titleRect.h = panel->titleHeight + panel->borderThickness / 2;
	panel->titleTextImg->x = panel->titleRect.x + (panel->titleRect.w / 2) - (panel->titleTextImg->w / 2);
	panel->titleTextImg->y = panel->titleRect.y + (panel->titleRect.h / 2) - (panel->titleTextImg->h / 2);
	
	panel->bgLocalCenter.x = panel->background.w / 2;
	panel->bgLocalCenter.y = panel->background.h / 2;
	panel->bgGlobalCenter.x = panel->bgLocalCenter.x + panel->background.x;
	panel->bgGlobalCenter.y = panel->bgLocalCenter.y + panel->background.y;
	
	panel->resizeBar_horizontal.x = panel->border.x + panel->border.w - panel->resizeBar_horizontal.w;
	panel->resizeBar_horizontal.y = panel->border.y + panel->border.h - panel->resizeBar_horizontal.h;
	panel->resizeBar_vertical.x = panel->border.x + panel->border.w - panel->resizeBar_vertical.w;
	panel->resizeBar_vertical.y = panel->border.y + panel->border.h - panel->resizeBar_vertical.h;	
	
	panel->horizontalScrollbarBG.x = panel->background.x;
	panel->horizontalScrollbarBG.y = panel->background.y + panel->background.h;
	panel->horizontalScrollbarBG.w = panel->background.w;
	panel->horizontalScrollbar.x = panel->horizontalScrollbarBG.x - (panel->x_scroll_offset * (panel->background.w / (double)panel->masterControlRect.w));
	panel->horizontalScrollbar.y = panel->horizontalScrollbarBG.y;
	
	panel->verticalScrollbarBG.x = panel->background.x + panel->background.w - panel->verticalScrollbarBG.w;
	panel->verticalScrollbarBG.y = panel->background.y;
	panel->verticalScrollbarBG.h = panel->background.h - panel->verticalScrollbarBG.w;
	panel->verticalScrollbar.x = panel->verticalScrollbarBG.x;
	panel->verticalScrollbar.y = panel->verticalScrollbarBG.y;
}

void SGE_SetActiveWindowPanel(SGE_WindowPanel *panel)
{
	int i = 0;
	
	/* Sets all but the given panel as active */
	for(i = 0; i < currentStateControls->panelCount; i++)
	{
		currentStateControls->panels[i]->isActive = false;
	}
	panel->isActive = true;
	SGE_SendActivePanelToTop();
}

SGE_WindowPanel *SGE_GetActiveWindowPanel()
{
	return currentStateControls->panels[currentStateControls->panelCount - 1];
}

void SGE_SendActivePanelToTop()
{
	int i = 0;
	SGE_WindowPanel *tempPanel = NULL;

	SGE_WindowPanel **panels = currentStateControls->panels;
	int panelCount = currentStateControls->panelCount;
	
	/* Finds the active panel and sends it to the top of the panels stack */
	for(i = 0; i < panelCount - 1; i++)
	{
		if(panels[i]->isActive)
		{
			//SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Changing active Panel to %s", panels[i]->titleStr);
			
			/* Store the active panel in a temp location */
			tempPanel = panels[i];
			
			/* Send everything after the active panel index down one index to make space at the top of the stack */
			int j = 0;
			for(j = i; j < panelCount; j++)
			{
				//printPanels();
				panels[j] = panels[j + 1];
			}
			
			/* Send the active panel to the top */
			panels[panelCount - 1] = tempPanel;
			tempPanel = NULL;
			
			//printPanels();
			SGE_GUI_LogPrintLine(SGE_LOG_DEBUG, "Active Panel changed to %s", panels[panelCount - 1]->titleStr);
			
			/* Stop searching since the active panel has been found */
			break;
		}
	}
	
	/* Reindex panels */
	for(i = 0; i < panelCount; i++)
	{
		panels[i]->index = i;
	}
	
	printPanelsStr();
}

void SGE_WindowPanelToggleMinimized(SGE_WindowPanel *panel)
{
	if(panel->isMinimized)
	{
		panel->isMinimized = false;
		panel->minimizeButton->buttonImg->rotation = 0;
		
		panel->border.w = panel->temp_border_w;
		panel->border.h = panel->temp_border_h;
		
		panel->background.w = panel->temp_background_w;
		panel->background.h = panel->temp_background_h;
		
		panel->resizeBar_horizontal.x = panel->border.x + panel->border.w - panel->resizeBar_horizontal.w;
		panel->resizeBar_horizontal.y = panel->border.y + panel->border.h - panel->resizeBar_horizontal.h;
		panel->resizeBar_vertical.x = panel->border.x + panel->border.w - panel->resizeBar_vertical.w;
		panel->resizeBar_vertical.y = panel->border.y + panel->border.h - panel->resizeBar_vertical.h;
		
		panel->bgLocalCenter.x = panel->background.w / 2;
		panel->bgLocalCenter.y = panel->background.h / 2;
		panel->bgGlobalCenter.x = panel->bgLocalCenter.x + panel->background.x;
		panel->bgGlobalCenter.y = panel->bgLocalCenter.y + panel->background.y;
		
		panel->horizontalScrollbarEnabled = panel->temp_horizontalScrollbarEnabled;
		panel->horizontalScrollbarBG.x = panel->background.x;
		panel->horizontalScrollbarBG.y = panel->background.y + panel->background.h - panel->horizontalScrollbarBG.h;
		panel->horizontalScrollbar.y = panel->horizontalScrollbarBG.y;
		
		panel->verticalScrollbarEnabled = panel->temp_verticalScrollbarEnabled;
		panel->verticalScrollbarBG.y = panel->background.y;
		panel->verticalScrollbarBG.x = panel->background.x + panel->background.w - panel->verticalScrollbarBG.w;
		panel->verticalScrollbar.x = panel->verticalScrollbarBG.x;
		
		panel->onMaximize(panel->onMaximize_data);
	}
	else
	{
		panel->isMinimized = true;
		panel->minimizeButton->buttonImg->rotation = -90;
		
		panel->temp_border_w = panel->border.w;
		panel->temp_border_h = panel->border.h;
		panel->temp_background_w = panel->background.w;
		panel->temp_background_h = panel->background.h;
		panel->temp_horizontalScrollbarEnabled = panel->horizontalScrollbarEnabled;
		panel->temp_verticalScrollbarEnabled = panel->verticalScrollbarEnabled;
		
		panel->border.w = panel->titleRect.w;
		panel->border.h = panel->titleRect.h;
		
		panel->background.h = 0;
		
		panel->resizeBar_horizontal.x = panel->border.x + panel->border.w - panel->resizeBar_horizontal.w;
		panel->resizeBar_horizontal.y = panel->border.y + panel->border.h - panel->resizeBar_horizontal.h;
		panel->resizeBar_vertical.x = panel->border.x + panel->border.w - panel->resizeBar_vertical.w;
		panel->resizeBar_vertical.y = panel->border.y + panel->border.h - panel->resizeBar_vertical.h;
		
		panel->bgLocalCenter.x = panel->background.w / 2;
		panel->bgLocalCenter.y = panel->background.h / 2;
		panel->bgGlobalCenter.x = panel->bgLocalCenter.x + panel->background.x;
		panel->bgGlobalCenter.y = panel->bgLocalCenter.y + panel->background.y;
		
		panel->horizontalScrollbarEnabled = false;
		panel->horizontalScrollbarBG.x = panel->background.x;
		panel->horizontalScrollbarBG.y = panel->background.y + panel->background.h - panel->horizontalScrollbarBG.h;
		panel->horizontalScrollbar.y = panel->horizontalScrollbarBG.y;
		
		panel->verticalScrollbarEnabled = false;
		panel->verticalScrollbarBG.y = panel->background.y;
		panel->verticalScrollbarBG.x = panel->background.x + panel->background.w - panel->verticalScrollbarBG.w;
		panel->verticalScrollbar.x = panel->verticalScrollbarBG.x;
		
		panel->onMinimize(panel->onMinimize_data);
	}
}

/* Called whenever a control is added to a panel or when a control changes position */
void SGE_WindowPanelCalculateMCR(SGE_WindowPanel *panel, SDL_Rect boundBox)
{
	if(panel->controlCount == 1)
	{
		panel->masterControlRect = boundBox;
	}
	else
	{
		if(boundBox.x < panel->masterControlRect.x)
		{
			panel->masterControlRect.w += panel->masterControlRect.x - boundBox.x;
			panel->masterControlRect.x = boundBox.x;
		}
		
		if(boundBox.x + boundBox.w > panel->masterControlRect.x + panel->masterControlRect.w)
			panel->masterControlRect.w += (boundBox.x + boundBox.w) - (panel->masterControlRect.x + panel->masterControlRect.w);
		
		if(boundBox.y < panel->masterControlRect.y)
		{
			panel->masterControlRect.h += panel->masterControlRect.y - boundBox.y;
			panel->masterControlRect.y = boundBox.y;
		}
		
		if(boundBox.y + boundBox.h > panel->masterControlRect.y + panel->masterControlRect.h)
			panel->masterControlRect.h += (boundBox.y + boundBox.h) - (panel->masterControlRect.y + panel->masterControlRect.h);
	}
	
	SGE_WindowPanelShouldEnableHorizontalScroll(panel);
	SGE_WindowPanelShouldEnableVerticalScroll(panel);
}

void SGE_WindowPanelShouldEnableHorizontalScroll(SGE_WindowPanel *panel)
{
	/* Check if horizontal scrollbar should appear */
	if(panel->masterControlRect.x + panel->masterControlRect.w > panel->background.x + panel->background.w)
	{
		panel->scroll_dx = (panel->masterControlRect.x + panel->masterControlRect.w) - (panel->background.x + panel->background.w);
		panel->horizontalScrollbarEnabled = true;

		if(panel->verticalScrollbarEnabled)
			panel->horizontalScrollbarBG.w = panel->background.w - panel->horizontalScrollbarBG.h;
		else
			panel->horizontalScrollbarBG.w = panel->background.w;
		
		/* Calculate scrollbar width */
		int temp_w = panel->masterControlRect.w + (panel->masterControlRect.x - panel->background.x);
		double scrBarWidth = panel->horizontalScrollbarBG.w - ((panel->scroll_dx / (double)temp_w) * panel->horizontalScrollbarBG.w);
		panel->horizontalScrollbar.w = SDL_ceil(scrBarWidth);

		/* Calculate scrollbar x position */
		double scrlBarPos = panel->horizontalScrollbarBG.x - ((panel->x_scroll_offset / temp_w) * panel->horizontalScrollbarBG.w);
		panel->horizontalScrollbar.x = SDL_ceil(scrlBarPos);

		if((panel->horizontalScrollbar.x + panel->horizontalScrollbar.w) > (panel->horizontalScrollbarBG.x + panel->horizontalScrollbarBG.w))
		{
			panel->isScrolling_horizontal = true;
		}
	}
	else
	{
		panel->x_scroll_offset = 0;
		panel->horizontalScrollbar.x = panel->horizontalScrollbarBG.x;
		panel->horizontalScrollbarEnabled = false;
	}
}

void SGE_WindowPanelShouldEnableVerticalScroll(SGE_WindowPanel *panel)
{
	/* Check if vertical scrollbar should appear */
	if(panel->masterControlRect.y + panel->masterControlRect.h > panel->background.y + panel->background.h)
	{
		panel->scroll_dy = (panel->masterControlRect.y + panel->masterControlRect.h) - (panel->background.y + panel->background.h);
		panel->verticalScrollbarEnabled = true;

		if(panel->horizontalScrollbarEnabled)
			panel->verticalScrollbarBG.h = panel->background.h - panel->verticalScrollbarBG.w;
		else
			panel->verticalScrollbarBG.h = panel->background.h;
		
		/* Calculate vertical scrollbar width */
		int temp_h = panel->masterControlRect.h + (panel->masterControlRect.y - panel->background.y);
		double scrBarWidth = panel->verticalScrollbarBG.h - ((panel->scroll_dy / (double)temp_h) * panel->verticalScrollbarBG.h);
		panel->verticalScrollbar.h = SDL_ceil(scrBarWidth);

		/* Calculate vertical scrollbar y position */
		double scrlBarPos = panel->verticalScrollbarBG.y - ((panel->y_scroll_offset / temp_h) * panel->verticalScrollbarBG.h);
		panel->verticalScrollbar.y = SDL_ceil(scrlBarPos);

		if((panel->verticalScrollbar.y + panel->verticalScrollbar.h) > (panel->verticalScrollbarBG.y + panel->verticalScrollbarBG.h))
		{
			panel->isScrolling_vertical = true;
		}
	}
	else
	{
		panel->y_scroll_offset = 0;
		panel->verticalScrollbar.y = panel->verticalScrollbarBG.y;
		panel->verticalScrollbarEnabled = false;
	}
}

/* Layouting functions for easy GUI Layouting */
SDL_Point SGE_ControlGetPositionNextTo(SDL_Rect controlBoundBox, SDL_Rect targetBoundBox, SGE_ControlDirection direction, int spacing_x, int spacing_y)
{
	SDL_Point position = {0, 0};
	
	switch(direction)
	{
		case SGE_CONTROL_DIRECTION_UP:
		position.x = targetBoundBox.x + spacing_x;
		position.y = targetBoundBox.y - controlBoundBox.h - spacing_y;
		break;
		
		case SGE_CONTROL_DIRECTION_UP_CENTERED:
		position.x = targetBoundBox.x + (targetBoundBox.w / 2) - (controlBoundBox.w / 2) + spacing_x;
		position.y = targetBoundBox.y - controlBoundBox.h - spacing_y;
		break;
		
		case SGE_CONTROL_DIRECTION_DOWN:
		position.x = targetBoundBox.x + spacing_x;
		position.y = targetBoundBox.y + targetBoundBox.h + spacing_y;
		break;
		
		case SGE_CONTROL_DIRECTION_DOWN_CENTERED:
		position.x = targetBoundBox.x + (targetBoundBox.w / 2) - (controlBoundBox.w / 2) + spacing_x;
		position.y = targetBoundBox.y + targetBoundBox.h + spacing_y;
		break;
		
		case SGE_CONTROL_DIRECTION_LEFT:
		position.x = targetBoundBox.x - controlBoundBox.w - spacing_x;
		position.y = targetBoundBox.y + spacing_y;
		break;
		
		case SGE_CONTROL_DIRECTION_LEFT_CENTERED:
		position.x = targetBoundBox.x - controlBoundBox.w - spacing_x;
		position.y = targetBoundBox.y + (targetBoundBox.h / 2) - (controlBoundBox.h / 2) + spacing_y;
		break;
		
		case SGE_CONTROL_DIRECTION_RIGHT:
		position.x = targetBoundBox.x + targetBoundBox.w + spacing_x;
		position.y = targetBoundBox.y + spacing_y;
		break;
		
		case SGE_CONTROL_DIRECTION_RIGHT_CENTERED:
		position.x = targetBoundBox.x + targetBoundBox.w + spacing_x;
		position.y = targetBoundBox.y + (targetBoundBox.h / 2) - (controlBoundBox.h / 2) + spacing_y;
		break;
	}
	
	return position;
}

void SGE_ButtonSetPositionNextTo(SGE_Button *button, SDL_Rect targetBoundBox, SGE_ControlDirection direction, int spacing_x, int spacing_y)
{
	SDL_Point position = SGE_ControlGetPositionNextTo(button->boundBox, targetBoundBox, direction, spacing_x, spacing_y);
	if(button->parentPanel != NULL)
	{
		position.x -= button->parentPanel->background.x;
		position.y -= button->parentPanel->background.y;
	}
	SGE_ButtonSetPosition(button, position.x, position.y);
}

void SGE_CheckBoxSetPositionNextTo(SGE_CheckBox *checkBox, SDL_Rect targetBoundBox, SGE_ControlDirection direction, int spacing_x, int spacing_y)
{
	SDL_Point position = SGE_ControlGetPositionNextTo(checkBox->boundBox, targetBoundBox, direction, spacing_x, spacing_y);
	if(checkBox->parentPanel != NULL)
	{
		position.x -= checkBox->parentPanel->background.x;
		position.y -= checkBox->parentPanel->background.y;
	}
	SGE_CheckBoxSetPosition(checkBox, position.x, position.y);
}

void SGE_TextLabelSetPositionNextTo(SGE_TextLabel *label, SDL_Rect targetBoundBox, SGE_ControlDirection direction, int spacing_x, int spacing_y)
{
	SDL_Point position = SGE_ControlGetPositionNextTo(label->boundBox, targetBoundBox, direction, spacing_x, spacing_y);
	if(label->parentPanel != NULL)
	{
		position.x -= label->parentPanel->background.x;
		position.y -= label->parentPanel->background.y;
	}
	SGE_TextLabelSetPosition(label, position.x, position.y);
}

void SGE_SliderSetPositionNextTo(SGE_Slider *slider, SDL_Rect targetBoundBox, SGE_ControlDirection direction, int spacing_x, int spacing_y)
{
	SDL_Point position = SGE_ControlGetPositionNextTo(slider->boundBox, targetBoundBox, direction, spacing_x, spacing_y);
	if(slider->parentPanel != NULL)
	{
		position.x -= slider->parentPanel->background.x;
		position.y -= slider->parentPanel->background.y;
	}
	SGE_SliderSetPosition(slider, position.x, position.y);
}

void SGE_TextInputBoxSetPositionNextTo(SGE_TextInputBox *textInputBox, SDL_Rect targetBoundBox, SGE_ControlDirection direction, int spacing_x, int spacing_y)
{
	SDL_Point position = SGE_ControlGetPositionNextTo(textInputBox->boundBox, targetBoundBox, direction, spacing_x, spacing_y);
	if(textInputBox->parentPanel != NULL)
	{
		position.x -= textInputBox->parentPanel->background.x;
		position.y -= textInputBox->parentPanel->background.y;
	}
	SGE_TextInputBoxSetPosition(textInputBox, position.x, position.y);
}

void SGE_ListBoxSetPositionNextTo(SGE_ListBox *listBox, SDL_Rect targetBoundBox, SGE_ControlDirection direction, int spacing_x, int spacing_y)
{
	SDL_Point position = SGE_ControlGetPositionNextTo(listBox->boundBox, targetBoundBox, direction, spacing_x, spacing_y);
	if(listBox->parentPanel != NULL)
	{
		position.x -= listBox->parentPanel->background.x;
		position.y -= listBox->parentPanel->background.y;
	}
	SGE_ListBoxSetPosition(listBox, position.x, position.y);
}

void SGE_WindowPanelSetPositionNextTo(SGE_WindowPanel *panel, SDL_Rect targetBoundBox, SGE_ControlDirection direction, int spacing_x, int spacing_y)
{
	SDL_Point position = SGE_ControlGetPositionNextTo(panel->border, targetBoundBox, direction, spacing_x, spacing_y);
	SGE_WindowPanelSetPosition(panel, position.x, position.y);
}
