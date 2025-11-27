# AWS QDMI Device Setup Guide

This project implements the QDMI interface for AWS Braket. To run the examples and tests, you need to configure your AWS credentials.

## Prerequisites

1.  **AWS Account**: You need an active AWS account.
2.  **AWS Braket Enabled**: Ensure AWS Braket is enabled in your account.
3.  **AWS CLI (Optional)**: Useful for verifying credentials.

## Configuring Credentials

The AWS SDK for C++ uses the standard AWS credential chain. You can provide credentials in several ways:

### Option 1: Environment Variables (Recommended for Testing)

Set the following environment variables in your terminal:

```bash
export AWS_ACCESS_KEY_ID="your_access_key_id"
export AWS_SECRET_ACCESS_KEY="your_secret_access_key"
export AWS_SESSION_TOKEN="your_session_token" # Optional, if using temporary credentials
export AWS_DEFAULT_REGION="us-east-1" # Or your preferred region
```

### Option 2: AWS Credentials File

Create or edit the file `~/.aws/credentials` (Linux/macOS) or `%USERPROFILE%\.aws\credentials` (Windows):

```ini
[default]
aws_access_key_id = your_access_key_id
aws_secret_access_key = your_secret_access_key
```

And `~/.aws/config`:

```ini
[default]
region = us-east-1
output = json
```

## Running the Example

The example program (`aws_qdmi_example`) attempts to connect to the AWS Braket SV1 simulator.

1.  Build the project:
    ```bash
    mkdir build && cd build
    cmake ..
    make
    ```

2.  Run the example:
    ```bash
    ./aws_qdmi_example
    ```

If successful, you should see the device name and qubit count.

## Troubleshooting

*   **"Failed to get device"**: This usually means your credentials are missing or invalid, or you don't have permission to access AWS Braket.
*   **"Failed to initialize session: -7"**: This indicates the `GetDevice` call failed, likely due to credentials. Check the error message printed to stderr.
