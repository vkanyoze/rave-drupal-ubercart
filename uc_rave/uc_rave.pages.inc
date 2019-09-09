<?php


function uc_rave_complete($cart_id = 0) {
  if (empty($_GET) || !isset($_GET['txref'])) {
    //drupal_goto(variable_get('uc_mypayment_cancel_return_url', 'cart'));

  }else{

    $txref = check_plain($_GET['txref']);
    $payment_amount = check_plain($_GET['amount']);
    $payment_currency = check_plain($_GET['currency']);
    $order_id = check_plain($_GET['order_id']);


    function requery($requeryCount){
      $stagingUrl = 'https://ravesandboxapi.flutterwave.com/';
      $liveUrl = 'https://api.ravepay.co/';
      $apiLink = $stagingUrl;

      $env = variable_get('uc_rave_env', 'staging');
      $secretKey = variable_get('uc_rave_test_secretkey', '');
      if ($env == 'live') {
        $apiLink = $liveUrl;
        $secretKey = variable_get('uc_rave_live_secretkey', '');
      }

      $requeryCount++;

      $data = array(
        'txref' => $_GET['txref'],
        'SECKEY' => $secretKey,
        'last_attempt' => '1'
        // 'only_successful' => '1'
      );
    // make request to endpoint.
      $data_string = json_encode($data);
      $ch = curl_init();
      curl_setopt($ch, CURLOPT_URL, $apiLink . 'flwv3-pug/getpaidx/api/v2/verify');
      curl_setopt($ch, CURLOPT_CUSTOMREQUEST, "POST");
      curl_setopt($ch, CURLOPT_POSTFIELDS, $data_string);
      curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
      curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
      curl_setopt($ch, CURLOPT_HTTPHEADER, array('Content-Type: application/json'));
      $response = curl_exec($ch);
      $header_size = curl_getinfo($ch, CURLINFO_HEADER_SIZE);
      $header = substr($response, 0, $header_size);
      $body = substr($response, $header_size);
      curl_close($ch);
      $resp = json_decode($response, false);

      if ($resp && $resp->status === "success") {
        if ($resp && $resp->data && $resp->data->status === "successful") {
          return $resp->data;
        } elseif ($resp && $resp->data && $resp->data->status === "failed") {
          return false;
        } else {
                // I will requery again here. Just incase we have some devs that cannot setup a queue for requery. I don't like this.
          if ($requeryCount > 4) {
            return false;
          } else {
            sleep(3);
            return requery($requeryCount);
          }
        }
      } else {
        if ($requeryCount > 4) {
          return false;
        } else {
          sleep(3);
          return requery($requeryCount);
        }
      }
    }

    function getBaseUrl(){
        // output: /myproject/index.php
        $currentPath = $_SERVER['PHP_SELF'];

        // output: Array ( [dirname] => /myproject [basename] => index.php [extension] => php [filename] => index )
        $pathInfo = pathinfo($currentPath);

        // output: localhost
        $hostName = $_SERVER['HTTP_HOST'];

        // output: http://
        $protocol = strtolower(substr($_SERVER["SERVER_PROTOCOL"],0,5))=='https://'?'https://':'http://';

        // return: http://localhost/myproject/
        return $protocol.$hostName.$pathInfo['dirname']."/";
    }

    $verification = requery(0);

    $order = uc_order_load($order_id);

    $debugmessage = "";
    

    if($verification == false){

        uc_order_comment_save($order->order_id, 0, t("The customer cancelled payment."), 'order', 'canceled');
        drupal_set_message($debugmessage . t('Your payment was cancelled or your transaction failed. Please feel free to continue shopping or contact us for assistance.'));
        unset($_SESSION['cart_order']);
        drupal_goto(variable_get('uc_mypayment_cancel_return_url', 'cart'));
      

    } else {

      $email = $order->primary_email;;
      $payment_amount  = $verification->amount;
      $currency = $verification->currency;

      if ($order->order_total == $payment_amount && $payment_currency == $currency) {
        $comment = t('Rave transaction ID: @PayId , Email: @Email', array('@PayId' => $txref,'@Email' => $email));

        uc_payment_enter($order->order_id, 'Rave', $payment_amount, $order->uid, NULL, $comment);

        $cartresult = uc_cart_complete_sale($order, variable_get('uc_new_customer_login', FALSE));

        uc_order_comment_save($order->order_id, 0, t('Payment of @amount @currency paid through Rave. Transaction ID: @PayId ', array('@PayId' => $txref,'@amount' =>   $payment_amount , '@currency' => $payment_currency)), 'order', 'payment_received');

        uc_order_comment_save($order->order_id, 0, t('Rave reported a payment of @amount @currency', array('@amount' =>   $payment_amount , '@currency' => $currency)));

        drupal_set_message($debugmessage . t('Your payment was completed.'));

        $orderurl = getBaseUrl()."user/".$order->uid.'/orders/'.$order->order_id;
        $result = "<p> Your payment transaction reference is :#".$txref.".</p>";

        return $result.$cartresult['#message'];

      } else{

        uc_order_comment_save($order->order_id, 0, t("The customer cancelled payment."), 'order', 'canceled' );
        drupal_set_message($debugmessage .t('Invalid amount was paid. Please feel free to continue shopping or contact us for assistance.'));
        unset($_SESSION['cart_order']);
        drupal_goto(variable_get('uc_mypayment_cancel_return_url', 'cart'));

      }

    }
  }

}


