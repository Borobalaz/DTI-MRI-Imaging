from manim import *
import numpy as np
import random


class QSpaceRange(ThreeDScene):
    """
    Visualizes the finite and discretely sampled region of q-space
    measured in a diffusion-weighted MR acquisition.

    Render:
        manim -pqh q_space_range.py QSpaceRange
    """

    def construct(self):
        self.set_camera_orientation(phi=68 * DEGREES, theta=-45 * DEGREES)
        self.camera.background_color = WHITE

        axes = ThreeDAxes(
            x_range=[-3.4, 3.4, 1],
            y_range=[-3.4, 3.4, 1],
            z_range=[-3.4, 3.4, 1],
            x_length=6.2,
            y_length=6.2,
            z_length=6.2,
            #change color of axes
            axis_config={
                "color": BLACK, 
                "stroke_width": 1.5, 
            },
        )

        axis_labels = axes.get_axis_labels(
            MathTex("q_x"),
            MathTex("q_y"),
            MathTex("q_z"),
        )
        axis_labels.set_color(BLACK)

        self.add(axes, axis_labels)

        # The ideal Fourier relationship is defined over all q-space.
        qmax_sphere = Sphere(
            center=axes.c2p(0, 0, 0),
            radius=2.35,
            resolution=(28, 56),
            fill_opacity=0.11,
            stroke_opacity=0.42,
            # style it for white background
            fill_color=BLUE,
            stroke_color=BLUE
        )

        self.add(qmax_sphere)
        
        points = self.fibonacci_sphere(20, radius=2.35)
        #vectors from origin to each point on the sphere
        vectors = VGroup(*[
            Arrow3D(
                start=axes.c2p(0, 0, 0),
                end=axes.c2p(*p * random.uniform(0.1, 3.0)), #random length
                thickness=0.008,
                #color to red
                color=RED,
            )
            for p in points
        ])
        self.add(vectors)

    def make_qmax_arrow(self, axes):
        return Arrow3D(
            start=axes.c2p(0, 0, 0),
            end=axes.c2p(2.35, 0, 0),
            thickness=0.018,
        )

    @staticmethod
    def fibonacci_sphere(n, radius=1.0):
        """Approximately uniform unit vectors on a sphere."""
        points = []
        golden_angle = np.pi * (3 - np.sqrt(5))

        for i in range(n):
            z = 1 - 2 * (i + 0.5) / n
            radius_xy = np.sqrt(max(0, 1 - z * z))
            theta = golden_angle * i
            x = radius_xy * np.cos(theta)
            y = radius_xy * np.sin(theta)
            points.append(np.array([x, y, z]))

        return [radius * p for p in points]
