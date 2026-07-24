"""
Real-time rendering using MatPlotLib.
"""

import time

import matplotlib
import matplotlib.pyplot as plt
import numpy as np

# https://matplotlib.org/stable/users/explain/animations/blitting.html
class PointsInSpace:
    def __init__(
        self,
        title=None,
        xlabel=None,
        ylabel=None,
        xlim=None,
        ylim=None,
        hide_axis=False,
        tight=False,
        enable_grid=False,
        enable_legend=False,
    ):
        matplotlib.rcParams["toolbar"] = "None"
        self.fig = plt.figure()
        self.ax = self.fig.add_subplot()

        # if xlim is None:

        if ylim is None:
            ylim = xlim
        
        self.ax.set_ylim(ylim)
        self.ax.set_xlim(xlim)
        # self.ax.autoscale_view()
        # self.ax.set_aspect("equal", adjustable="box")

        self.ax.set_title(title)
        self.ax.set_xlabel(xlabel)
        self.ax.set_ylabel(ylabel)

        if hide_axis:
            self.ax.set_axis_off()
            
        # From https://stackoverflow.com/a/47893499
        # This also lowers performance for some reason
        # if tight:
        #     self.fig.tight_layout(pad=0)
        #     w, h = self.fig.get_size_inches()
        #     x1, x2 = self.ax.get_xlim()
        #     y1, y2 = self.ax.get_ylim()
        #     self.fig.set_size_inches(
        #         2 * 1.08 * w, 2 * self.ax.get_aspect() * (y2 - y1) / (x2 - x1) * w
        #     )
            
        if enable_grid:
            self.ax.grid()
            
        self.enable_legend = enable_legend

        self.plot_dict = {}

        plt.show(block=False)
        plt.pause(0.1)
        self.bg = self.fig.canvas.copy_from_bbox(self.fig.bbox)

    def register_plot(self, label, m="o", alpha=1):
        (points,) = self.ax.plot([], [], m, alpha=alpha, animated=True, label=label)
        self.plot_dict[label] = points

    def start_drawing(self):
        self.fig.canvas.restore_region(self.bg)

    def draw_points(self, label, x, y):
        # self.ax.set_xlim([min(x)-0.1, max(x)+0.1])
        # self.ax.set_xlim([np.amin(x)-1, np.max(x)+1])
        if label not in self.plot_dict:
            raise ValueError(f"Plot {label} not registered")
        self.plot_dict[label].set_data(x, y)
        self.ax.draw_artist(self.plot_dict[label])

    def end_drawing(self, delay=0):
        if self.enable_legend:
            self.ax.draw_artist(self.ax.legend())
        self.fig.canvas.blit(self.fig.bbox)
        self.fig.canvas.flush_events()

        if delay > 0:
            plt.pause(delay)


if __name__ == "__main__":
    pp = PointsInSpace("Dots circling", enable_legend=True)
    pp.register_plot("dots")
    pp.register_plot("lines", m="-")
    frame_count = 50000
    num_dots = 15
    speed = 0.005

    tic = time.time()
    for i in range(frame_count):
        t = (2 * np.pi / num_dots) * np.arange(num_dots)
        t += i * speed

        x = np.cos(t) * np.cos(4 * t)
        y = np.sin(t) * np.cos(4 * t)

        pp.start_drawing()
        pp.draw_points("dots", x, y)
        pp.draw_points("lines", x, y)
        pp.end_drawing()

    print(f"Average FPS: {frame_count / (time.time() - tic)}")