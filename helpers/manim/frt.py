from manim import *
import numpy as np


class FunkRadonTransformVisualization(ThreeDScene):
    """
    Static visualization of the Funk–Radon transform

        Ff(xi) = (1 / 2pi) ∫_{<xi, eta> = 0} f(eta) dλ(eta)

    Render the final frame with, for example:

        manim -pqh funk_radon.py FunkRadonTransformVisualization

    The scene contains no animations, so the preview is the desired final frame.
    """

    def construct(self):
        # ------------------------------------------------------------
        # Camera and layout
        # ------------------------------------------------------------
        self.set_camera_orientation(
            phi=68 * DEGREES,
            theta=-42 * DEGREES,
            zoom=0.82,
        )

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
        center = axes.c2p(-3, -3, 0)
        radius = 2.15

        centerPoint = Dot3D(
            point=center,
            color=BLACK,
            radius=0.1
        )

        sphere = Sphere(
            center=center,
            radius=radius,
            resolution=(28, 56),
            fill_opacity=0.11,
            stroke_opacity=0.42,
            fill_color=BLUE,
            stroke_color=BLUE
        )

        # Arbitrary direction for xi
        xiDirection = normalize(np.array([1.0, 1.3, 0.8]))

        greatCircle = Circle(
            radius=radius,
            color=RED,
            stroke_width=2.5,
        )

        # A Circle initially lies in the xy-plane, with normal vector OUT = [0, 0, 1].
        # Rotate it so its normal becomes xiDirection.
        greatCircle.apply_matrix(z_to_vector(xiDirection))
        greatCircle.move_to(center)

        xiVector = Arrow3D(
            start=center,
            end=center + radius * xiDirection,
            thickness=0.008,
            color=RED,
        )

        xiLabel = MathTex(r"\xi", color=BLACK)
        xiLabel.move_to(center + (radius + 0.3) * xiDirection)
        
        u = z_to_vector(xiDirection) @ RIGHT
        v = z_to_vector(xiDirection) @ UP

        arcLabel = MathTex(r"\lambda(\eta)", color=BLACK)
        arcLabel.move_to(
            center + (radius + 0.35) * normalize(-u - v)
        )
        
        # Great circle function plot
        twodAxes = Axes(
            x_range=[0, TAU, PI/2],
            y_range=[0, 1.2, 0.2],
            x_length=4.2,
            y_length=2.4,
            axis_config={
                "color": BLACK,
                "stroke_width": 2,
            },
        )
        x_label = MathTex(r"\lambda(\eta)", color=BLACK)
        y_label = MathTex(r"f(\eta)", color=BLACK)

        labels = twodAxes.get_axis_labels(
            x_label=x_label,
            y_label=y_label,
        )


        # A made-up function on the great circle
        graph = twodAxes.plot(
            lambda t: (
                0.65
                + 0.22*np.cos(2*t - 0.5)
                + 0.12*np.sin(5*t)
            ),
            x_range=[0, TAU],
            color=BLUE,
            stroke_width=3,
        )
        
        average = 0.65

        average_line = twodAxes.plot(
            lambda x: average,
            x_range=[0, TAU],
            color=GREEN,
            stroke_width=2,
            stroke_opacity=0.8,
        )
        frtText = MathTex(r"FRT(f(\xi))", color=GREEN)
        frtText.move_to(twodAxes.c2p(TAU + 1.5, average))
        
        redLine = Line(
            start=twodAxes.c2p(0, 0),
            end=twodAxes.c2p(TAU, 0),
            color=RED,
            stroke_width=2.5
        )
        axes_group = VGroup(twodAxes, graph, redLine, labels, average_line, frtText)
        axes_group.move_to(np.array([3, 0, 3]))
        
        self.add_fixed_in_frame_mobjects(axes_group)
        self.add(axes_group)
        self.add(sphere)
        self.add(greatCircle)
        self.add(xiVector)
        self.add_fixed_orientation_mobjects(xiLabel)
        self.add(xiLabel)
        self.add_fixed_orientation_mobjects(arcLabel)
        self.add(arcLabel)
        self.add(centerPoint)
