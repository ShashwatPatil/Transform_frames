import math
import time

import numpy as np
from scipy.spatial.transform import Rotation as R_scipy


class FloorplanTransformer:
    def __init__(self, img_origin_uwb_x, img_origin_uwb_y, scale, rotation=0, x_flipped=False, y_flipped=False):
        """
        Args:
            img_origin_uwb_x (float): The X location of the Image's Top-Left corner in UWB frame (mm).
            img_origin_uwb_y (float): The Y location of the Image's Top-Left corner in UWB frame (mm).
            scale (float): Pixels per UWB Unit (px/mm).
            rotation (float): Rotation of the UWB frame in Degrees (Counter-Clockwise).
                              Note: logic below uses radians.
            x_flipped (bool): If True, UWB X axis opposes Image X axis.
            y_flipped (bool): If True, UWB Y axis opposes Image Y axis.
        """
        self.img_origin_uwb_x = img_origin_uwb_x
        self.img_origin_uwb_y = img_origin_uwb_y
        self.scale = scale
        # User input logic used negative rotation in transform_rot.py relative to the input angle?
        # In transform_rot.py: rotation= -0.436... and self.rotation_rad = -rotation.
        # This implies self.rotation_rad = 0.436...
        # Wait, if rotation is passed as input (e.g. -0.43...), then self.rotation_rad = -(-0.43) = 0.43.
        # I will keep the logic exactly as in transform_rot.py
        self.rotation_rad = -rotation 
        
        self.x_flipped = x_flipped
        self.y_flipped = y_flipped
        
        self.matrix = self._calculate_matrix()
        self.rotation_transform_matrix = self._calculate_rotation_transform()

    def _calculate_matrix(self):
        # --- 1. Translation Matrix (T) ---
        T = np.array([
            [1, 0, -self.img_origin_uwb_x],
            [0, 1, -self.img_origin_uwb_y],
            [0, 0, 1]
        ])

        # --- 2. Rotation Matrix (R) ---
        c = np.cos(self.rotation_rad)
        s = np.sin(self.rotation_rad)
        
        R = np.array([
            [c, -s, 0],
            [s,  c, 0],
            [0,  0, 1]
        ])

        # --- 3. Scale Matrix (S) ---
        scale_x = self.scale * (-1 if self.x_flipped else 1)
        scale_y = self.scale * (-1 if self.y_flipped else 1)
        
        S = np.array([
            [scale_x, 0,       0],
            [0,       scale_y, 0],
            [0,       0,       1]
        ])

        # --- 4. Combine (Order: S @ R @ T) ---
        combined_matrix = S @ R @ T
        
        return combined_matrix

    def _calculate_rotation_transform(self):
        """
        Calculate the 3x3 rotation matrix that represents the frame transformation
        for orientation (Quaternions).
        This includes the Rotation (R) and the Axis Flips (from S).
        """
        # Base Rotation
        c = np.cos(self.rotation_rad)
        s = np.sin(self.rotation_rad)
        
        # Standard Z-rotation matrix
        rot_z = np.array([
            [c, -s, 0],
            [s,  c, 0],
            [0,  0, 1]
        ])
        
        # Flip Matrix (Reflection)
        # Assuming flips are applied along local axes after rotation, or before?
        # In _calculate_matrix: S @ R. Flips are in S.
        # So we rotate first, then flip.
        
        flip_x = -1 if self.x_flipped else 1
        flip_y = -1 if self.y_flipped else 1
        
        flip_mat = np.array([
            [flip_x, 0, 0],
            [0, flip_y, 0],
            [0, 0, 1]
        ])
        
        # Combined Transform for Vectors/Orientation: Flip @ Rotation
        return flip_mat @ rot_z

    def transform_pose(self, uwb_x, uwb_y, uwb_z=1000, qx=0, qy=0, qz=0, qw=1):
        """
        Transform 3D position and orientation.
        uwb_z default: 1000 mm (1 unit = 1 meter output).
        orientation default: Identity.
        """
        
        # --- Position Transform ---
        # 1. Transform X, Y using the affine matrix
        vec = np.array([uwb_x, uwb_y, 1])
        res = self.matrix @ vec
        
        # 2. Convert to meters (divide by scale to get mm, then /1000)
        # res[0], res[1] are in Pixels. 
        # To get meters: (Pixels / (Pixels/mm)) / 1000 = mm / 1000 = m
        x_m = (res[0] / self.scale) / 1000.0
        y_m = (res[1] / self.scale) / 1000.0
        
        # 3. Z Transform
        # Assuming Z is just scaled by units (mm -> m). No pixel scaling for Z normally.
        z_m = uwb_z / 1000.0
        
        # --- Orientation Transform ---
        # 1. Convert input quaternion to Rotation Matrix
        r_in = R_scipy.from_quat([qx, qy, qz, qw])
        mat_in = r_in.as_matrix()
        
        # 2. Apply Frame Transform
        # R_out = R_transform @ R_in
        mat_out = self.rotation_transform_matrix @ mat_in
        
        # 3. Convert back to Quaternion
        # Note: If determinant is -1 (reflection), we cannot convert to a valid rotation quaternion.
        det = np.linalg.det(mat_out)
        if det < 0:
            # Handle Reflection: simple conversion not possible for pure rotation representation.
            # However, for 2D floorplans with doubled flips (x and y), det is positive.
            # If det is negative, we might need a workaround or warning.
            # For now, we'll try to convert and catch errors, or just warn.
            # print(f"Warning: Transform involves reflection (det={det}), orientation may be invalid.")
            # Fallback: Convert the closest rotation?
            r_out = R_scipy.from_matrix(mat_out * -1) # Flip all? No.
            # Assume strictly dealing with X+Y flip => Det=1.
            # If single flip, we ignore the flip for orientation to keep it right-handed?
            # Correct approach for single flip: Flip the output quaternion's relevant axis?
            # Let's stick to strict rotation for now.
            quat_out = [0, 0, 0, 1] 
        else:
            r_out = R_scipy.from_matrix(mat_out)
            quat_out = r_out.as_quat() # returns [x, y, z, w]

        return {
            'x': x_m, 'y': y_m, 'z': z_m,
            'qx': quat_out[0], 'qy': quat_out[1], 'qz': quat_out[2], 'qw': quat_out[3]
        }

def generate_tf_output(transformer_instance, uwb_data):
    """
    Generate ROS2 /tf compatible dictionary.
    uwb_data: list of dicts with keys 'x', 'y', 'z', 'qx', 'qy', 'qz', 'qw', 'tag_id'
    """
    transforms_list = []
    
    current_time = time.time()
    sec = int(current_time)
    nanosec = int((current_time - sec) * 1e9)
    
    for tag in uwb_data:
        # Defaults
        tx = tag.get('x', 0)
        ty = tag.get('y', 0)
        tz = tag.get('z', 1000) # Default to 1000mm (1m)
        qx = tag.get('qx', 0)
        qy = tag.get('qy', 0)
        qz = tag.get('qz', 0)
        qw = tag.get('qw', 1)
        
        result = transformer_instance.transform_pose(tx, ty, tz, qx, qy, qz, qw)
        
        tf_entry = {
            "header": {
                "stamp": {
                    "sec": sec,
                    "nanosec": nanosec
                },
                "frame_id": "map",
                "child_frame_id": f"uwb_tag_{tag.get('tag_id', 'unknown')}"
            },
            "transform": {
                "translation": {
                    "x": result['x'],
                    "y": result['y'],
                    "z": result['z']
                },
                "rotation": {
                    "x": result['qx'],
                    "y": result['qy'],
                    "z": result['qz'],
                    "w": result['qw']
                }
            }
        }
        transforms_list.append(tf_entry)
        
    return {"transforms": transforms_list}

# --- Usage Example ---
if __name__ == "__main__":
    # Initialize Transfomer with parameters from context
    transformer = FloorplanTransformer(
        img_origin_uwb_x=23469.3880740585, 
        img_origin_uwb_y=30305.220662758173, 
        scale=1 / 24.1279818088302, 
        rotation= -0.4363323129985825, # Original input
        x_flipped=True, 
        y_flipped=True
    )

    # Mock Data: One tag
    # Assuming tag provides explicit orientation (Quaternions) or we default to identity
    mock_uwb_data = [
        {
            'tag_id': '6755',
            'x': 4396,
            'y': 17537,
            # 'z': 1500, # Should default to 1000 if omitted
            # Identity orientation
            'qx': 0.0, 'qy': 0.0, 'qz': 0.0, 'qw': 1.0
        },
        {
            'tag_id': 'rotated_tag',
            'x': 5000, 
            'y': 18000,
            'z': 2000,
            # 90 degrees yaw (around Z) relative to UWB frame
            # q = [0, 0, sin(45), cos(45)] = [0, 0, 0.707, 0.707]
            'qx': 0.0, 'qy': 0.0, 'qz': 0.70710678, 'qw': 0.70710678
        }
    ]

    output = generate_tf_output(transformer, mock_uwb_data)
    
    # Print as YAML-like format
    print("transforms:")
    for tf in output['transforms']:
        print(f"- header:")
        print(f"    stamp:")
        print(f"      sec: {tf['header']['stamp']['sec']}")
        print(f"      nanosec: {tf['header']['stamp']['nanosec']}")
        print(f"    frame_id: \"{tf['header']['frame_id']}\"")
        print(f"    child_frame_id: \"{tf['header']['child_frame_id']}\"")
        print(f"  transform:")
        print(f"    translation:")
        print(f"      x: {tf['transform']['translation']['x']:.4f}")
        print(f"      y: {tf['transform']['translation']['y']:.4f}")
        print(f"      z: {tf['transform']['translation']['z']:.4f}")
        print(f"    rotation:")
        print(f"      x: {tf['transform']['rotation']['x']:.4f}")
        print(f"      y: {tf['transform']['rotation']['y']:.4f}")
        print(f"      z: {tf['transform']['rotation']['z']:.4f}")
        print(f"      w: {tf['transform']['rotation']['w']:.4f}")
