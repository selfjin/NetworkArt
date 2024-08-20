#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")


#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "Network.h"

void SOCK_ERROR_PRINT(const char* string)
{
    printf("Faild : %s,  ", string);
    int value = WSAGetLastError();
    printf("Error Code : %d\n", value);
}

//-------------------------------------------------------------
//  전역 변수
//-------------------------------------------------------------
int Connect_Flag = 0;

SOCKET Client_Socket;

RingBuffer g_RingBuffer_Recv(10000);
RingBuffer g_RingBuffer_Send(10000);




//-------------------------------------------------------------
// 함수
//-------------------------------------------------------------


void ReadProcedure(HWND hWnd)
{
    HDC hdc;
    PAINTSTRUCT ps;
    HPEN hPenOld, hPen;

    while(1)
    {
        char buffer[MESSAGE_SIZE] = { 0, };
        int recvLen = recv(Client_Socket, buffer, HEADER_SIZE, 0);

        if (recvLen < HEADER_SIZE)
        {
            if (recvLen == SOCKET_ERROR)
            {
                if (WSAGetLastError() == WSAEWOULDBLOCK)
                {
                    return;
                }
                else
                {
                    MessageBox(NULL, L"RST OR FIN", L" RST OR FIN", MB_OK);
                }
            }
            g_RingBuffer_Recv.Enqueue(buffer, recvLen);
            return;
        }

        // Header 사이즈 정상
        g_RingBuffer_Recv.Enqueue(buffer, HEADER_SIZE);
        
        // payLoad 체크
        char payloadBuffer[PAYLOAD_SIZE] = { 0, };
        
        int payloadLen = recv(Client_Socket, payloadBuffer, PAYLOAD_SIZE, 0);

        if (payloadLen < PAYLOAD_SIZE)
        {
            g_RingBuffer_Recv.Enqueue(buffer, payloadLen);
            return;
        }

        g_RingBuffer_Recv.Enqueue(payloadBuffer, PAYLOAD_SIZE);
        
        ALL_PACKET packet;

        g_RingBuffer_Recv.Dequeue((char*)&packet, MESSAGE_SIZE);
        
//-------------------------------------------------------------
// 그리기
//-------------------------------------------------------------
        hdc = GetDC(hWnd);

        hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
        hPenOld = (HPEN)SelectObject(hdc, hPen);

        // 현재 위치와 이전 위치를 잇는 선을 그림
        MoveToEx(hdc, packet.startX, packet.startY, nullptr);
        LineTo(hdc, packet.endX, packet.endY);

        SelectObject(hdc, hPenOld);
        DeleteObject(hPen);

        ReleaseDC(hWnd, hdc);

    }
}







LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"DrawWindowClass";
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowW(L"DrawWindowClass", L"Draw with Mouse", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);


    //---------------------------------------------------------------------/
    //							네트워크
    //--------------------------------------------------------------------/

    // 0. Socket_Error_Code_retVal

    int select_retVal;
    int connect_retVal;
    int nonblocking_retVal;



    WSAData wsaData;
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        SOCK_ERROR_PRINT("WSAStartUp()");
        return 0;
    }

    Client_Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (Client_Socket == INVALID_SOCKET)
    {
        SOCK_ERROR_PRINT("socket()");
        return 0;
    }

    u_long nonblocking_option = 1;
    nonblocking_retVal = ioctlsocket(Client_Socket, FIONBIO, &nonblocking_option);

    if (nonblocking_retVal == SOCKET_ERROR)
    {
        SOCK_ERROR_PRINT("ioctlsocket()");
        return 0;
    }

    // 1. Server Setting

    SOCKADDR_IN serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    InetPton(AF_INET, L"127.0.0.1", &serverAddr.sin_addr);
    serverAddr.sin_port = htons(SERVER_PORT);

    // 2. Select
    select_retVal = WSAAsyncSelect(Client_Socket, hWnd, WM_SOCKET, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE);

    if (select_retVal == SOCKET_ERROR)
    {
        SOCK_ERROR_PRINT("select");
        return 0;
    }

    // 3. Connect 
    connect_retVal = connect(Client_Socket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));

    if (connect_retVal == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSAEWOULDBLOCK)
        {
            int retVal = WSAGetLastError();
            return 0;
        }
    }



    // 3. Message Loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static POINT ptPrevious = { 3, 3 };
    HDC hdc;
    PAINTSTRUCT ps;
    HPEN hPenOld, hPen;
    static BOOL bDrawing = FALSE;

    switch (message)
    {
    case WM_SOCKET:
    {
        if (WSAGETSELECTERROR(lParam))
        {
            int WM_error = WSAGetLastError();
            DebugBreak;
        }

        switch (WSAGETSELECTEVENT(lParam))
        {
        case FD_CONNECT:
            Connect_Flag = 1;
            break;
        case FD_WRITE:
            
            break;
        case FD_READ:
            ReadProcedure(hWnd);
            break;
        case FD_CLOSE:
            
            break;
        }
        
        break;
    }
    case WM_LBUTTONDOWN:
        // 마우스 왼쪽 버튼을 누르면 드래그 시작

        ptPrevious.x = LOWORD(lParam);
        ptPrevious.y = HIWORD(lParam);
        bDrawing = TRUE;
        break;

    case WM_MOUSEMOVE:
    {
        if (bDrawing)
        {

            if (Connect_Flag != 1)
            {
                break;
            }

            stHeader header = { 16 };
            st_DRAW_PACKET payload = {
                (int)ptPrevious.x, (int)ptPrevious.y,
                (int)(LOWORD(lParam)), (int)(HIWORD(lParam))
            };

            ptPrevious.x = LOWORD(lParam);
            ptPrevious.y = HIWORD(lParam);
            

            if (g_RingBuffer_Send.getCapacity() - g_RingBuffer_Send.getSize() >= 18)
            {
                g_RingBuffer_Send.Enqueue((char*)&header, sizeof(header));
                g_RingBuffer_Send.Enqueue((char*)&payload, sizeof(payload));
            }

            char buffer[MESSAGE_SIZE] = { 0, };

            if (g_RingBuffer_Send.getSize() >= MESSAGE_SIZE)
            {
                int peekSize = g_RingBuffer_Send.peek(buffer, MESSAGE_SIZE);
                int retVal = send(Client_Socket, buffer, MESSAGE_SIZE, 0);

                g_RingBuffer_Send.Dequeue(buffer, peekSize);
            }
            
            
        }
        break;
        
    }
        
        

    case WM_LBUTTONUP:
        // 마우스 왼쪽 버튼을 놓으면 드래그 종료
        bDrawing = FALSE;
        break;

    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        // 필요한 경우 여기에서 윈도우를 다시 그릴 수 있습니다.
        EndPaint(hWnd, &ps);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}



//hdc = GetDC(hWnd);

            //hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
            //hPenOld = (HPEN)SelectObject(hdc, hPen);

            //// 현재 위치와 이전 위치를 잇는 선을 그림
            //MoveToEx(hdc, ptPrevious.x, ptPrevious.y, nullptr);
            //LineTo(hdc, LOWORD(lParam), HIWORD(lParam));

            //SelectObject(hdc, hPenOld);
            //DeleteObject(hPen);

            //ReleaseDC(hWnd, hdc);

            //// 이전 위치를 현재 위치로 업데이트
            //ptPrevious.x = LOWORD(lParam);
            //ptPrevious.y = HIWORD(lParam);