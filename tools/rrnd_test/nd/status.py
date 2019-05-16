class RRNDStatus(object):

    UNCHANGED = 0x0000  # Successful, GS data unchanged
    SUCCESS = 0x0000    # Successful, GS data unchanged
    UPDATED = 0x0001    # GS data has been updated
    DELETE = 0x0002     # GS should be deleted

    UPDATE_CONTACTS = 0x0010       # The predicted contacts
    UPDATE_LOCATOR_DATA = 0x0020   # The locator struct
    UPDATE_NEIGHBOR_INFO = 0x0040  # Neighbor list or NBF
    UPDATE_AVAILABILITY = 0x0080   # Availability Pattern

    FAILED = 0x0008  # Combined with flags below

    FAILURE_PREDICTOR = 0x1000
    FAILURE_LOCATOR = 0x2000
    FAILURE_VALIDATOR = 0x0400

    FAILURE_OVERFLOW = 0x0800
