import json
import boto3
import base64


def lambda_handler(event, context):
    print(event)
    bucket_name = "battendance-bucket"
    rekognition = boto3.client("rekognition")
    sns = boto3.client("sns")
    s3 = boto3.client("s3")
    list_objects_response = s3.list_objects_v2(Bucket=bucket_name)
    object_list = list_objects_response["Contents"]
    
    response_body = {
        "personIsRecognised": False,
        "personName": "",
        "error": ""
    }
    
    try:
        body = json.loads(event["body"])
        base64_image = body["image"]["data"]
        image = base64.b64decode(base64_image)
    except:
        print("could not extract image from request body")
        response_body['error'] = "failed to extract image from body of request"
        return {
            'statusCode': 500,
            'headers': {
            'Access-Control-Allow-Headers': 'Content-Type',
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'OPTIONS,POST,GET'
            },
            'body': json.dumps(response_body)
        }
    try:
        s3.put_object(Bucket=bucket_name,
                    Body=image,
                    ContentType="image/jpeg",
                    Key="temporary-image")
    except:
        print("could not put object into s3 bucket")
        response_body["error"] = "failed to insert image into s3 bucket"
        return {
            'statusCode': 500,
            'headers': {
            'Access-Control-Allow-Headers': 'Content-Type',
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'OPTIONS,POST,GET'
            },
            'body': json.dumps(response_body)
        }

    for item in object_list:
        face = item["Key"]
        rekognition_response = {}
        try:
            rekognition_response = rekognition.compare_faces(
                SimilarityThreshold=90,
                SourceImage={
                    "S3Object": {
                        "Bucket": bucket_name,
                        "Name": face
                    },
                },
                TargetImage={
                    "S3Object": {
                        "Bucket": bucket_name,
                        "Name": "temporary-image"
                    },
                },
            )
        except:
            print(rekognition_response)
            print("face comparison failed")
            s3.delete_object(Bucket=bucket_name,
                            Key="temporary-image")
            response_body['error'] = "failed to compare faces"
            return {
                'statusCode': 500,
                'headers': {
                    'Access-Control-Allow-Headers': 'Content-Type',
                    'Access-Control-Allow-Origin': '*',
                    'Access-Control-Allow-Methods': 'OPTIONS,POST,GET'
                },
                'body': json.dumps(response_body)
            }
        if rekognition_response['FaceMatches'] != []:
            response_body['personIsRecognised'] = True
            response_body['personName'] = face
            sns_response = sns.publish(Message=(face + " is present"),
                                      TopicArn="arn:aws:sns:ap-southeast-1:798051472288:IoTS-project")
            response_code = sns_response["ResponseMetadata"]["HTTPStatusCode"]
            if response_code != 200:
                print("publish unsuccessful, response code: " + response_code)
            else:
                print("publish successful")
            break
    s3.delete_object(Bucket=bucket_name,
                    Key="temporary-image")

    return {
        'statusCode': 200,
        'headers': {
            'Access-Control-Allow-Headers': 'Content-Type',
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'OPTIONS,POST,GET'
        },
        'body': json.dumps(response_body)
    }
