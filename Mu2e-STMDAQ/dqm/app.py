# Import required modules from Dash and standard libraries
import dash                                  # Dash core module
from dash import html, dcc, page_container, callback, Output, Input  # HTML and component container tools
import dash_mantine_components as dmc      # For alerts
import psutil                                # Used to pin process to CPU core
import os                                    # For environment variable access
import xml.etree.ElementTree as ET           # For parsing XML configuration files

from utils.config import get_xml_node_value
from worker_manager import manager
from workers import worker_raw_data, worker_baseline, worker_peaks, worker_cpu, worker_alarms
from flask import make_response

# Create the Dash app and enable page system
app = dash.Dash(__name__,
                use_pages=True,
                suppress_callback_exceptions=True,
                prevent_initial_callbacks=True,
                )

 # Attempt to extract the value of <stm><stmdaq_starting_core>
core_id = int(get_xml_node_value("stmdaq_starting_core"))
dqm_core_id = core_id+15

# Get channel for port number setting
expanded_path = os.path.expandvars("${HOSTNAME}")
if expanded_path == get_xml_node_value("ch0_host"):
    channel = 0
    name = "HPGe"
    port_num = 8050
    dqm_core_id += 24 #Numa 1 for HPGe
elif expanded_path == get_xml_node_value("ch1_host"):
    channel = 1
    name = "LaBr3"
    port_num = 8060
else:
    raise Exception("Not an STM data channel server")


# Attempt to pin this process to the specified CPU core
p = psutil.Process()  # Get current process
try:
    p.cpu_affinity([dqm_core_id])  # Set CPU affinity
    print(f"Dash DQM pinned to CPU core {dqm_core_id}")
except Exception as e:
    print(f"Warning: Could not set CPU affinity: {e}")

# Add workers and assign cores
manager.add_worker("raw_data", worker_raw_data, core_id=dqm_core_id+1)
manager.add_worker("baseline", worker_baseline, core_id=dqm_core_id+2)
manager.add_worker("peaks", worker_peaks, core_id=dqm_core_id+3)
manager.add_worker("cpu_speeds", worker_cpu, core_id=dqm_core_id+4)
manager.add_worker("alarms", worker_alarms, core_id=dqm_core_id+5)


# Top bar with title on the left and logos on the right
topbar = html.Div([
    # Left section: logo1 + DQM title + run info
    html.Div([
        html.Img(src="/assets/mu2e.png", className="logo-left"),
        html.Div([
            html.Div(rf"Mu2e STM DQM {name}", className="title"),
            #html.Div("Run XXXX Event XXX", className="subtitle")
        ])
    ], className="topbar-left"),

    # Right section: absolutely positioned container for logos 2 and 3
    html.Div([
        html.Img(src="/assets/mcr_new.png", className="logo-right"),
        html.Img(src="/assets/ucl_new.png", className="logo-right"),
        html.Img(src="/assets/uol.png", className="logo-right")
    ], className="topbar-right")
  ], className="topbar",
   style={
    "height": "80px",
    "alignItems": "center"}
)

# Navigation bar
navbar = html.Div([
    dcc.Link("Overview", href="/", className="nav-link"),
    dcc.Link("Raw data", href="/raw-data", className="nav-link"),
    dcc.Link("Baseline and noise", href="/adc-baseline", className="nav-link"),
    dcc.Link("Peak data", href="/peak-data", className="nav-link"),
    dcc.Link("DAQ info", href="/daq-info", className="nav-link"),
    dcc.Link("Logs", href="/logs", className="nav-link"),
    # html.Button("Clear Histos", id="clear-button", className="nav-button")
], className="navbar", id="navbar")

# App layout
app.layout = dmc.MantineProvider(
    theme = {
        "primaryColor": "blue",
        "colors": {
            "soft-red": [
                "#fff5f5", "#ffe3e3", "#ffc9c9", "#ffa8a8", "#ff8787",
                "#ff6b6b", "#fa5252", "#f03e3e", "#e03131", "#c92a2a"
            ],
        },
        "components": {
            "Notification" : {
                "styles" : {
                    "root": {
                        "backgroundColor": "var(--mantine-color-soft-red-6)",
                        "width": "600px",
                    },
                    "title": {
                        "color": "white",
                        "fontSize": '22px',
                        "fontWeight": 800
                    },
                    "description": {
                        "color": "white",
                        "fontSize": '18px',
                        "fontWeight": 500
                    },
                    "icon": {"display": "none"},
                },
            }
        }
    },

    children = [

        # Layout
        topbar,
        navbar,
        html.Div(className="page-content",
                 children=page_container),

        # Alerts
        dmc.NotificationContainer(id="notif-container",
                limit = 10,
                position = "top-center",
        ),

        dcc.Store(id="latest-alarm-time", data={"latest_alarm_time":0}),
        dcc.Interval(id="alarm-interval", interval=2000, n_intervals=0),

        dcc.Location(id="url", refresh=False),

        # Persistent store holding time history of baseline values
        # accessible in overview and baseline page
        dcc.Store(id="baseline-history", data={
            "x": [],
            "prev": [],
            "prev_rms": [],
            "avg": [],
            "avg_rms": [],
            "curr": [],
            "curr_rms": [],
            "last_timestamp": 0
        }),

        # Persistent store holding DAQ history
        dcc.Store(id="daq-history", data={
            "time": [],
            "temp": [],
            "dropped_packets": [],
        }),

    ]
)

@callback(
    Output("navbar", "children"),
    Input("url", "pathname")
)
def update_active_nav(pathname):
    links = [
        ("Overview",            "/"),
        ("Raw data",            "/raw-data"),
        ("Baseline and noise",  "/adc-baseline"),
        ("Peak data",           "/peak-data"),
        ("DAQ info",            "/daq-info"),
        ("Logs",                "/logs"),
    ]
    return [
        dcc.Link(
            label,
            href=href,
            className="nav-link active" if pathname == href else "nav-link"
        )
        for label, href in links
    ]

#atexit.register(shutdown)

@app.server.route('/status')
def get_status():
    return "hello!", 200, {
        "Content-Type": "text/plain; charset=utf-8",
        "Access-Control-Allow-Origin": "*",
        "Connection": "close"
    }

# @app.server.route('/status')
# def get_status():
#     response = make_response("hello!")
#     response.headers['Access-Control-Allow-Origin'] = 'http://localhost:30351'   
#     response.headers['Access-Control-Allow-Methods'] = 'GET' 
#     response.headers['Access-Control-Allow-Headers'] = 'Content-Type'
#     return response


# Start the Dash server
if __name__ == '__main__':
    app.run(debug=True, port=port_num, host='0.0.0.0')
    #app.run(debug=True, port=port_num)
    #app.run(debug=True, port=port_num, use_reloader=False)

