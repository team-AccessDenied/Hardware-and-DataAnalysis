USE WORKPLACE1;
SELECT Fname, Lname FROM EMPLOYEE WHERE ssn IN (
    SELECT ssn FROM WORKS_ON WHERE Pno IN (
        SELECT Pno FROM WORKS_ON
        WHERE ssn = (SELECT Essn FROM EMPLOYEE WHERE Fname = 'Franklin' AND Lname = 'Wong')
    )
);