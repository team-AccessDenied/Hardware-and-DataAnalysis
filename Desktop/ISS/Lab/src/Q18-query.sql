USE WORKPLACE1;
CREATE VIEW Department_Manager_Salary AS
SELECT D.Dname, M.Fname AS Manager_Fname, M.Lname AS Manager_Lname, M.Salary AS Manager_Salary
FROM DEPARTMENT D
JOIN EMPLOYEE M ON D.Mgr_ssn = M.Ssn;