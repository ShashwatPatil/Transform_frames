import math

import numpy as np


class FloorplanTransformer:
    def __init__(self, img_origin_uwb_x, img_origin_uwb_y, scale, rotation=0, x_flipped=False, y_flipped=False):
        """
        Args:
            img_origin_uwb_x (float): The X location of the Image's Top-Left corner in UWB frame. # for uwb posxy it is in mm
            img_origin_uwb_y (float): The Y location of the Image's Top-Left corner in UWB frame. # for uwb posxy it is in mm
            scale (float): Pixels per UWB Unit.
            rotation (float): Rotation of the UWB frame in Degrees (Counter-Clockwise).
            x_flipped (bool): If True, UWB X axis opposes Image X axis.
            y_flipped (bool): If True, UWB Y axis opposes Image Y axis.
        """
        self.img_origin_uwb_x = img_origin_uwb_x
        self.img_origin_uwb_y = img_origin_uwb_y
        self.scale = scale
        self.rotation_rad = -rotation
        # self.rotation_rad = np.radians(rotation) # Convert degrees to radians
        self.x_flipped = x_flipped
        self.y_flipped = y_flipped
        
        self.matrix = self._calculate_matrix()

    def _calculate_matrix(self):
        # --- 1. Translation Matrix (T) ---
        # Shifts points so the Image Origin becomes (0,0)
        # T = [1, 0, -Ox]
        #     [0, 1, -Oy]
        T = np.array([
            [1, 0, -self.img_origin_uwb_x],
            [0, 1, -self.img_origin_uwb_y],
            [0, 0, 1]
        ])

        # --- 2. Rotation Matrix (R) ---
        # Rotates the axes to align UWB with Image
        # Standard 2D Rotation Matrix
        c = np.cos(self.rotation_rad)
        s = np.sin(self.rotation_rad)
        
        R = np.array([
            [c, -s, 0],
            [s,  c, 0],
            [0,  0, 1]
        ])

        # --- 3. Scale Matrix (S) ---
        # Handles Scaling (px/m) and Axis Flipping
        scale_x = self.scale * (-1 if self.x_flipped else 1)
        scale_y = self.scale * (-1 if self.y_flipped else 1)
        
        S = np.array([
            [scale_x, 0,       0],
            [0,       scale_y, 0],
            [0,       0,       1]
        ])

        # --- 4. Combine (Order: S @ R @ T) ---
        # We apply T first (rightmost), then R, then S
        combined_matrix = S @ R @ T
        
        return combined_matrix

    def uwb_to_pixel(self, uwb_x, uwb_y):
        """Transform UWB (x,y) -> Pixel (x,y)"""
        vec = np.array([uwb_x, uwb_y, 1])
        res = self.matrix @ vec
        return (res[0]/self.scale)/1000 , (res[1]/self.scale)/1000 # Convert back to meters res[0], res[1] is in pixels
                                                                    # Divide by scale to get it in mm, then by 1000 to get meters 

    def pixel_to_uwb(self, pixel_x, pixel_y):
        """Transform Pixel (x,y) -> UWB (x,y)"""
        vec = np.array([pixel_x*1000*self.scale, pixel_y*1000*self.scale, 1]) # Convert meters to pixels for reverse transform
        # Use the inverse of the transformation matrix
        inv_matrix = np.linalg.inv(self.matrix)
        res = inv_matrix @ vec
        return res[0], res[1]  

transformer = FloorplanTransformer(
    img_origin_uwb_x=23469.3880740585, 
    img_origin_uwb_y=30305.220662758173, 
    scale=1 / 24.1279818088302, 
    rotation= -0.4363323129985825,       # <--- NEW: Rotation in radians
    x_flipped=True, 
    y_flipped=True
)

# Test with the origin point
px_origin, py_origin = transformer.uwb_to_pixel(4396, 17537)
print(f"Image Origin Check: ({px_origin:.2f}, {py_origin:.2f})")


# Test reverse transformation  # some problem with this fxn 
uwb_x_back, uwb_y_back = transformer.pixel_to_uwb(px_origin, py_origin)
print(f"Reverse Transform Check: ({uwb_x_back:.2f}, {uwb_y_back:.2f})")