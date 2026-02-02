import numpy as np


class FloorplanTransformer:
    def __init__(self, img_origin_uwb_x, img_origin_uwb_y, scale, x_flipped=False, y_flipped=False):
        """
        Args:
            img_origin_uwb_x (float): The X location of the Image's Top-Left corner in UWB frame.
            img_origin_uwb_y (float): The Y location of the Image's Top-Left corner in UWB frame.
            scale (float): Pixels per UWB Unit.
            x_flipped (bool): If True, UWB X axis opposes Image X axis.
            y_flipped (bool): If True, UWB Y axis opposes Image Y axis.
        """
        # Calculate directional Scale Factor (S)
        s_x = scale * (-1 if x_flipped else 1)
        s_y = scale * (-1 if y_flipped else 1)
        
        # Calculate Translation (T)
        t_x = -(s_x * img_origin_uwb_x)
        t_y = -(s_y * img_origin_uwb_y)
        
        # Store computed values
        self.computed_origin_pixel_x = t_x
        self.computed_origin_pixel_y = t_y

        # Construct transformation matrix once
        self.matrix = np.array([
            [s_x, 0,   t_x],
            [0,   s_y, t_y],
            [0,   0,   1]
        ], dtype=np.float32)

    def update_transform(self, img_origin_uwb_x, img_origin_uwb_y, scale, x_flipped=False, y_flipped=False):
        """
        Update the transformation matrix with new parameters.
        """
        # Calculate directional Scale Factor (S)
        s_x = scale * (-1 if x_flipped else 1)
        s_y = scale * (-1 if y_flipped else 1)
        
        # Calculate Translation (T)
        t_x = -(s_x * img_origin_uwb_x)
        t_y = -(s_y * img_origin_uwb_y)
        
        # Update computed values
        self.computed_origin_pixel_x = t_x
        self.computed_origin_pixel_y = t_y

        # Update transformation matrix
        self.matrix = np.array([
            [s_x, 0,   t_x],
            [0,   s_y, t_y],
            [0,   0,   1]
        ], dtype=np.float32)

    def uwb_to_pixel(self, uwb_x, uwb_y):
        """Transform single UWB point (x,y) -> Pixel (x,y)"""
        vec = np.array([uwb_x, uwb_y, 1], dtype=np.float32)
        res = self.matrix @ vec
        return res[0], res[1]

    def uwb_to_pixel_batch(self, uwb_x, uwb_y):
        """
        Transform multiple UWB points -> Pixel coordinates (vectorized)
        
        Args:
            uwb_x: array-like of X coordinates
            uwb_y: array-like of Y coordinates
            
        Returns:
            tuple: (pixel_x, pixel_y) as numpy arrays
        """
        # Convert to numpy arrays
        uwb_x = np.asarray(uwb_x, dtype=np.float32)
        uwb_y = np.asarray(uwb_y, dtype=np.float32)
        
        # Create homogeneous coordinates [N x 3]
        points = np.stack([uwb_x, uwb_y, np.ones_like(uwb_x)], axis=-1)
        
        # Transform: [N x 3] @ [3 x 3].T = [N x 3]
        transformed = points @ self.matrix.T
        
        # Return pixel coordinates
        return transformed[..., 0], transformed[..., 1]


# --- Verification ---
user_scale = 1 / 18.923757662947224

transformer = FloorplanTransformer(
    img_origin_uwb_x=-11892.415327226576,
    img_origin_uwb_y=780.5784747914149,
    scale=user_scale,
    x_flipped=True,
    y_flipped=False
)

print(f"Calculated Matrix Translation (Should be ~315, ~1645):")
print(f"X: {transformer.computed_origin_pixel_x:.2f}")
print(f"Y: {transformer.computed_origin_pixel_y:.2f}")

# Test single point
px, py = transformer.uwb_to_pixel(0, 0)

print((px/user_scale)/1000 , (py/user_scale)/1000)
print(f"\nUWB Tag is at Pixel: ({px:.2f}, {py:.2f})")

# Test batch transformation
uwb_points_x = [0, 200, -1000, 500, -5994.48]
uwb_points_y = [0, 100, 200, -500, 31132.12]
batch_px, batch_py = transformer.uwb_to_pixel_batch(uwb_points_x, uwb_points_y)
print(f"\nBatch transformation of {len(uwb_points_x)} points:")
for i in range(len(uwb_points_x)):
    print(f"  UWB ({uwb_points_x[i]:8.2f}, {uwb_points_y[i]:8.2f}) -> Pixel ({batch_px[i]:8.2f}, {batch_py[i]:8.2f})")